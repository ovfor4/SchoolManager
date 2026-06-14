#include "schoolmanager/application/DocumentFormats.h"

#include "schoolmanager/application/ApplicationError.h"
#include "schoolmanager/application/TemplateTextRenderer.h"
#include "schoolmanager/config/Constants.h"

#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm-extension_api.h>
#include <cmark-gfm.h>
#include <hpdf.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace schoolmanager::application {

namespace {

struct FormatProfile {
    std::string_view id;
    std::string_view label;
};

inline constexpr FormatProfile kOtherFormat{.id = "other", .label = "Other"};
inline constexpr FormatProfile kTextFormat{.id = "text", .label = "Text"};
inline constexpr FormatProfile kMarkdownFormat{.id = "markdown", .label = "Markdown"};
inline constexpr FormatProfile kPdfFormat{.id = "pdf", .label = "PDF"};

inline constexpr std::string_view kMarkdownMimeType = "text/markdown";
inline constexpr std::string_view kMarkdownAlternateMimeType = "text/x-markdown";
inline constexpr std::string_view kPdfMimeType = "application/pdf";
inline constexpr std::string_view kTextExtension = ".txt";
inline constexpr std::string_view kPdfExtension = ".pdf";
inline constexpr std::array<std::string_view, 2> kMarkdownExtensions{".md", ".markdown"};
inline constexpr std::array<std::string_view, 5> kGfmExtensions{
    "table",
    "strikethrough",
    "autolink",
    "tagfilter",
    "tasklist",
};

std::vector<std::string> strings(std::initializer_list<std::string_view> values)
{
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.emplace_back(value);
    }
    return result;
}

template <std::size_t Size>
std::vector<std::string> strings(const std::array<std::string_view, Size>& values)
{
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.emplace_back(value);
    }
    return result;
}

DocumentExportFormatInfo exportFormat(FormatProfile format,
                                      std::string_view extension,
                                      std::string_view mimeType,
                                      std::string_view implementationFormatId = {})
{
    return DocumentExportFormatInfo{
        .id = std::string(format.id),
        .label = std::string(format.label),
        .extension = std::string(extension),
        .mime_type = std::string(mimeType),
        .implementation_format_id = std::string(
            implementationFormatId.empty() ? format.id : implementationFormatId),
    };
}

DocumentSourceFormatInfo sourceFormat(FormatProfile format,
                                      std::vector<std::string> extensions,
                                      std::vector<std::string> mimeTypes,
                                      std::string_view defaultExportFormat,
                                      std::vector<std::string> exportFormatIds,
                                      std::string_view implementationFormatId = {},
                                      bool supportsStudentVariables = true)
{
    return DocumentSourceFormatInfo{
        .id = std::string(format.id),
        .label = std::string(format.label),
        .extensions = std::move(extensions),
        .mime_types = std::move(mimeTypes),
        .default_export_format = std::string(defaultExportFormat),
        .export_format_ids = std::move(exportFormatIds),
        .supports_student_variables = supportsStudentVariables,
        .implementation_format_id = std::string(
            implementationFormatId.empty() ? format.id : implementationFormatId),
    };
}

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string mimeTypeWithoutParameters(std::string_view value)
{
    auto text = std::string(value);
    const auto semicolon = text.find(';');
    if (semicolon != std::string::npos) {
        text.resize(semicolon);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.pop_back();
    }
    return lowerAscii(std::move(text));
}

std::string lowerExtension(std::string_view fileName)
{
    return lowerAscii(std::filesystem::path(std::string(fileName)).extension().string());
}

bool containsId(const std::vector<std::string>& values, std::string_view id)
{
    return std::ranges::any_of(values, [id](const auto& value) { return value == id; });
}

std::string nodeTypeName(cmark_node* node)
{
    if (const auto* type = cmark_node_get_type_string(node); type != nullptr && *type != '\0') {
        return type;
    }
    return "unknown";
}

[[noreturn]] void throwUnsupportedMarkdown(cmark_node* node)
{
    throw ApplicationError(ApplicationErrorCode::BadRequest,
                           "unsupported Markdown feature: " + nodeTypeName(node));
}

void validateMarkdownNode(cmark_node* node)
{
    switch (cmark_node_get_type(node)) {
    case CMARK_NODE_DOCUMENT:
    case CMARK_NODE_PARAGRAPH:
    case CMARK_NODE_HEADING:
    case CMARK_NODE_LIST:
    case CMARK_NODE_ITEM:
    case CMARK_NODE_CODE_BLOCK:
    case CMARK_NODE_TEXT:
    case CMARK_NODE_SOFTBREAK:
    case CMARK_NODE_LINEBREAK:
    case CMARK_NODE_CODE:
    case CMARK_NODE_EMPH:
    case CMARK_NODE_STRONG:
        break;
    default:
        throwUnsupportedMarkdown(node);
    }

    if (cmark_node_get_type(node) == CMARK_NODE_LIST &&
        cmark_node_get_list_type(node) == CMARK_NO_LIST) {
        throwUnsupportedMarkdown(node);
    }

    for (auto* child = cmark_node_first_child(node); child != nullptr;
         child = cmark_node_next(child)) {
        validateMarkdownNode(child);
    }
}

struct TextRun {
    std::string text;
    bool bold = false;
    bool italic = false;
    bool code = false;
};

struct InlineStyle {
    bool bold = false;
    bool italic = false;
    bool code = false;
};

enum class PdfBlockKind {
    Paragraph,
    Heading,
    Code,
};

struct PdfBlock {
    PdfBlockKind kind = PdfBlockKind::Paragraph;
    std::vector<TextRun> runs;
    int heading_level = 0;
    int list_depth = 0;
    std::string list_marker;
};

void appendRun(std::vector<TextRun>& runs, TextRun run)
{
    if (run.text.empty()) {
        return;
    }
    if (!runs.empty() && runs.back().bold == run.bold &&
        runs.back().italic == run.italic && runs.back().code == run.code) {
        runs.back().text += run.text;
        return;
    }
    runs.push_back(std::move(run));
}

std::string nodeLiteral(cmark_node* node)
{
    if (const auto* literal = cmark_node_get_literal(node); literal != nullptr) {
        return literal;
    }
    return {};
}

void collectInlineRuns(cmark_node* node, InlineStyle style, std::vector<TextRun>& runs)
{
    switch (cmark_node_get_type(node)) {
    case CMARK_NODE_TEXT:
        appendRun(runs,
                  TextRun{
                      .text = nodeLiteral(node),
                      .bold = style.bold,
                      .italic = style.italic,
                      .code = style.code,
                  });
        return;
    case CMARK_NODE_SOFTBREAK:
        appendRun(runs,
                  TextRun{
                      .text = " ",
                      .bold = style.bold,
                      .italic = style.italic,
                      .code = style.code,
                  });
        return;
    case CMARK_NODE_LINEBREAK:
        appendRun(runs,
                  TextRun{
                      .text = "\n",
                      .bold = style.bold,
                      .italic = style.italic,
                      .code = style.code,
                  });
        return;
    case CMARK_NODE_CODE:
        style.code = true;
        appendRun(runs,
                  TextRun{
                      .text = nodeLiteral(node),
                      .bold = style.bold,
                      .italic = style.italic,
                      .code = style.code,
                  });
        return;
    case CMARK_NODE_EMPH:
        style.italic = true;
        break;
    case CMARK_NODE_STRONG:
        style.bold = true;
        break;
    default:
        break;
    }

    for (auto* child = cmark_node_first_child(node); child != nullptr;
         child = cmark_node_next(child)) {
        collectInlineRuns(child, style, runs);
    }
}

void appendBlocksFromNode(cmark_node* node,
                          std::vector<PdfBlock>& blocks,
                          int listDepth = 0,
                          std::string listMarker = {});

void appendListBlocks(cmark_node* list, std::vector<PdfBlock>& blocks, int listDepth)
{
    const auto listType = cmark_node_get_list_type(list);
    int itemIndex = std::max(1, cmark_node_get_list_start(list));

    for (auto* item = cmark_node_first_child(list); item != nullptr; item = cmark_node_next(item)) {
        auto marker = listType == CMARK_ORDERED_LIST ? std::to_string(itemIndex++) + "." : "*";
        bool markerUsed = false;
        for (auto* child = cmark_node_first_child(item); child != nullptr;
             child = cmark_node_next(child)) {
            if (cmark_node_get_type(child) == CMARK_NODE_LIST) {
                appendBlocksFromNode(child, blocks, listDepth + 1);
                continue;
            }
            appendBlocksFromNode(child, blocks, listDepth, markerUsed ? std::string{} : marker);
            markerUsed = true;
        }
        if (!markerUsed) {
            blocks.push_back(PdfBlock{
                .kind = PdfBlockKind::Paragraph,
                .runs = {},
                .heading_level = 0,
                .list_depth = listDepth,
                .list_marker = std::move(marker),
            });
        }
    }
}

void appendBlocksFromNode(cmark_node* node,
                          std::vector<PdfBlock>& blocks,
                          int listDepth,
                          std::string listMarker)
{
    switch (cmark_node_get_type(node)) {
    case CMARK_NODE_DOCUMENT:
        for (auto* child = cmark_node_first_child(node); child != nullptr;
             child = cmark_node_next(child)) {
            appendBlocksFromNode(child, blocks, listDepth);
        }
        return;
    case CMARK_NODE_PARAGRAPH: {
        PdfBlock block{
            .kind = PdfBlockKind::Paragraph,
            .runs = {},
            .heading_level = 0,
            .list_depth = listDepth,
            .list_marker = std::move(listMarker),
        };
        collectInlineRuns(node, InlineStyle{}, block.runs);
        blocks.push_back(std::move(block));
        return;
    }
    case CMARK_NODE_HEADING: {
        PdfBlock block{
            .kind = PdfBlockKind::Heading,
            .runs = {},
            .heading_level = cmark_node_get_heading_level(node),
            .list_depth = listDepth,
            .list_marker = std::move(listMarker),
        };
        collectInlineRuns(node, InlineStyle{.bold = true}, block.runs);
        blocks.push_back(std::move(block));
        return;
    }
    case CMARK_NODE_CODE_BLOCK: {
        auto literal = nodeLiteral(node);
        PdfBlock block{
            .kind = PdfBlockKind::Code,
            .runs = {},
            .heading_level = 0,
            .list_depth = listDepth,
            .list_marker = std::move(listMarker),
        };
        appendRun(block.runs,
                  TextRun{
                      .text = std::move(literal),
                      .code = true,
                  });
        blocks.push_back(std::move(block));
        return;
    }
    case CMARK_NODE_LIST:
        appendListBlocks(node, blocks, listDepth);
        return;
    default:
        throwUnsupportedMarkdown(node);
    }
}

std::vector<PdfBlock> markdownBlocks(cmark_node* root)
{
    validateMarkdownNode(root);
    std::vector<PdfBlock> blocks;
    appendBlocksFromNode(root, blocks);
    return blocks;
}

struct LoadedFontName {
    std::string path;
    unsigned int collection_index = 0;
    bool collection = false;
};

std::optional<LoadedFontName> firstExistingFont(
    const std::vector<LoadedFontName>& candidates)
{
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate.path)) {
            return candidate;
        }
    }
    return std::nullopt;
}

const char* loadFontName(HPDF_Doc pdf, const LoadedFontName& font)
{
    return font.collection
               ? HPDF_LoadTTFontFromFile2(pdf,
                                          font.path.c_str(),
                                          font.collection_index,
                                          HPDF_TRUE)
               : HPDF_LoadTTFontFromFile(pdf, font.path.c_str(), HPDF_TRUE);
}

struct PdfFonts {
    HPDF_Font regular = nullptr;
    HPDF_Font bold = nullptr;
    HPDF_Font italic = nullptr;
    HPDF_Font bold_italic = nullptr;
    HPDF_Font mono = nullptr;
};

void checkHpdf(HPDF_STATUS status, std::string_view action)
{
    if (status != HPDF_OK) {
        throw std::runtime_error("PDF generation failed while " + std::string(action));
    }
}

PdfFonts loadFonts(HPDF_Doc pdf)
{
    checkHpdf(HPDF_UseUTFEncodings(pdf), "enabling UTF-8 encodings");

    const auto regular = firstExistingFont({
        LoadedFontName{.path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"},
    });
    const auto bold = firstExistingFont({
        LoadedFontName{.path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
    });
    const auto italic = firstExistingFont({
        LoadedFontName{.path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf"},
    });
    const auto boldItalic = firstExistingFont({
        LoadedFontName{.path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf"},
    });
    const auto mono = firstExistingFont({
        LoadedFontName{.path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"},
    });

    if (regular) {
        const auto* regularName = loadFontName(pdf, *regular);
        const auto* boldName = bold ? loadFontName(pdf, *bold) : regularName;
        const auto* italicName = italic ? loadFontName(pdf, *italic) : regularName;
        const auto* boldItalicName = boldItalic ? loadFontName(pdf, *boldItalic) : boldName;
        const auto* monoName = mono ? loadFontName(pdf, *mono) : regularName;
        if (regularName != nullptr && boldName != nullptr && italicName != nullptr &&
            boldItalicName != nullptr && monoName != nullptr) {
            auto fonts = PdfFonts{
                .regular = HPDF_GetFont(pdf, regularName, "UTF-8"),
                .bold = HPDF_GetFont(pdf, boldName, "UTF-8"),
                .italic = HPDF_GetFont(pdf, italicName, "UTF-8"),
                .bold_italic = HPDF_GetFont(pdf, boldItalicName, "UTF-8"),
                .mono = HPDF_GetFont(pdf, monoName, "UTF-8"),
            };
            if (fonts.regular != nullptr && fonts.bold != nullptr && fonts.italic != nullptr &&
                fonts.bold_italic != nullptr && fonts.mono != nullptr) {
                return fonts;
            }
        }
    }

    HPDF_ResetError(pdf);
    return PdfFonts{
        .regular = HPDF_GetFont(pdf, "Helvetica", nullptr),
        .bold = HPDF_GetFont(pdf, "Helvetica-Bold", nullptr),
        .italic = HPDF_GetFont(pdf, "Helvetica-Oblique", nullptr),
        .bold_italic = HPDF_GetFont(pdf, "Helvetica-BoldOblique", nullptr),
        .mono = HPDF_GetFont(pdf, "Courier", nullptr),
    };
}

HPDF_Font fontForRun(const PdfFonts& fonts, const TextRun& run)
{
    if (run.code) {
        return fonts.mono;
    }
    if (run.bold && run.italic) {
        return fonts.bold_italic;
    }
    if (run.bold) {
        return fonts.bold;
    }
    if (run.italic) {
        return fonts.italic;
    }
    return fonts.regular;
}

std::size_t utf8CodepointSize(unsigned char first)
{
    if ((first & 0x80U) == 0U) {
        return 1;
    }
    if ((first & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((first & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((first & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 1;
}

struct TextToken {
    std::string text;
    bool newline = false;
    bool space = false;
};

std::vector<TextToken> tokensForRun(const TextRun& run)
{
    std::vector<TextToken> tokens;
    for (std::size_t index = 0; index < run.text.size();) {
        const auto ch = static_cast<unsigned char>(run.text[index]);
        if (run.text[index] == '\n') {
            tokens.push_back(TextToken{.text = {}, .newline = true});
            ++index;
            continue;
        }
        if (std::isspace(ch) != 0) {
            while (index < run.text.size() && run.text[index] != '\n' &&
                   std::isspace(static_cast<unsigned char>(run.text[index])) != 0) {
                ++index;
            }
            tokens.push_back(TextToken{.text = " ", .space = true});
            continue;
        }

        const auto start = index;
        while (index < run.text.size()) {
            const auto current = static_cast<unsigned char>(run.text[index]);
            if (run.text[index] == '\n' || std::isspace(current) != 0) {
                break;
            }
            index += std::min(utf8CodepointSize(current), run.text.size() - index);
        }
        tokens.push_back(TextToken{.text = run.text.substr(start, index - start)});
    }
    return tokens;
}

class PdfWriter {
  public:
    explicit PdfWriter(const std::vector<PdfBlock>& blocks)
        : pdf_(HPDF_New(nullptr, nullptr))
    {
        if (pdf_ == nullptr) {
            throw std::runtime_error("failed to create PDF document");
        }
        fonts_ = loadFonts(pdf_);
        if (!fonts_.regular || !fonts_.bold || !fonts_.italic || !fonts_.bold_italic ||
            !fonts_.mono) {
            throw std::runtime_error("failed to load PDF fonts");
        }
        addPage();
        for (const auto& block : blocks) {
            writeBlock(block);
        }
    }

    PdfWriter(const PdfWriter&) = delete;
    PdfWriter& operator=(const PdfWriter&) = delete;

    ~PdfWriter()
    {
        if (pdf_ != nullptr) {
            HPDF_Free(pdf_);
        }
    }

    std::string content()
    {
        checkHpdf(HPDF_SaveToStream(pdf_), "saving PDF");
        auto size = HPDF_GetStreamSize(pdf_);
        std::string output(size, '\0');
        checkHpdf(HPDF_ReadFromStream(pdf_,
                                      reinterpret_cast<HPDF_BYTE*>(output.data()),
                                      &size),
                  "reading PDF");
        output.resize(size);
        return output;
    }

  private:
    struct LineRun {
        TextRun run;
        HPDF_REAL width = 0;
    };

    HPDF_Doc pdf_ = nullptr;
    HPDF_Page page_ = nullptr;
    PdfFonts fonts_;
    HPDF_REAL page_width_ = 0;
    HPDF_REAL page_height_ = 0;
    HPDF_REAL y_ = 0;

    static constexpr HPDF_REAL margin_ = 54;
    static constexpr HPDF_REAL listIndent_ = 20;
    static constexpr HPDF_REAL markerWidth_ = 18;

    void addPage()
    {
        page_ = HPDF_AddPage(pdf_);
        if (page_ == nullptr) {
            throw std::runtime_error("failed to add PDF page");
        }
        checkHpdf(HPDF_Page_SetSize(page_, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT),
                  "setting PDF page size");
        page_width_ = HPDF_Page_GetWidth(page_);
        page_height_ = HPDF_Page_GetHeight(page_);
        y_ = page_height_ - margin_;
    }

    void ensureSpace(HPDF_REAL height)
    {
        if (y_ - height < margin_) {
            addPage();
        }
    }

    HPDF_REAL textWidth(const TextRun& run, HPDF_REAL fontSize) const
    {
        const auto font = fontForRun(fonts_, run);
        const auto width = HPDF_Font_TextWidth(
            font,
            reinterpret_cast<const HPDF_BYTE*>(run.text.data()),
            static_cast<HPDF_UINT>(run.text.size()));
        return static_cast<HPDF_REAL>(width.width) * fontSize / 1000.0F;
    }

    void drawText(const TextRun& run, HPDF_REAL fontSize, HPDF_REAL x, HPDF_REAL y)
    {
        if (run.text.empty()) {
            return;
        }
        checkHpdf(HPDF_Page_BeginText(page_), "starting PDF text");
        checkHpdf(HPDF_Page_SetFontAndSize(page_, fontForRun(fonts_, run), fontSize),
                  "setting PDF font");
        checkHpdf(HPDF_Page_TextOut(page_, x, y, run.text.c_str()), "drawing PDF text");
        checkHpdf(HPDF_Page_EndText(page_), "ending PDF text");
    }

    void drawLine(const std::vector<LineRun>& line,
                  HPDF_REAL fontSize,
                  HPDF_REAL x,
                  HPDF_REAL lineHeight,
                  const std::string& marker,
                  bool drawMarker)
    {
        ensureSpace(lineHeight);
        if (drawMarker && !marker.empty()) {
            drawText(TextRun{.text = marker, .bold = true}, fontSize, x, y_);
        }

        auto cursor = x + (marker.empty() ? 0 : markerWidth_);
        for (const auto& item : line) {
            drawText(item.run, fontSize, cursor, y_);
            cursor += item.width;
        }
        y_ -= lineHeight;
    }

    void appendLineRun(std::vector<LineRun>& line, LineRun item)
    {
        if (item.run.text.empty()) {
            return;
        }
        if (!line.empty() && line.back().run.bold == item.run.bold &&
            line.back().run.italic == item.run.italic &&
            line.back().run.code == item.run.code) {
            line.back().run.text += item.run.text;
            line.back().width += item.width;
            return;
        }
        line.push_back(std::move(item));
    }

    void writeWrappedRuns(const std::vector<TextRun>& runs,
                          HPDF_REAL fontSize,
                          HPDF_REAL lineHeight,
                          HPDF_REAL x,
                          HPDF_REAL maxWidth,
                          const std::string& marker)
    {
        std::vector<LineRun> line;
        HPDF_REAL lineWidth = 0;
        bool markerPending = true;

        const auto flushLine = [&]() {
            drawLine(line, fontSize, x, lineHeight, marker, markerPending);
            line.clear();
            lineWidth = 0;
            markerPending = false;
        };

        const auto appendToken = [&](const TextRun& run, std::string token) {
            TextRun tokenRun = run;
            tokenRun.text = std::move(token);
            const auto width = textWidth(tokenRun, fontSize);
            appendLineRun(line, LineRun{.run = std::move(tokenRun), .width = width});
            lineWidth += width;
        };

        for (const auto& run : runs) {
            for (const auto& token : tokensForRun(run)) {
                if (token.newline) {
                    flushLine();
                    continue;
                }
                if (token.space && line.empty()) {
                    continue;
                }

                TextRun tokenRun = run;
                tokenRun.text = token.text;
                auto tokenWidth = textWidth(tokenRun, fontSize);
                if (lineWidth + tokenWidth <= maxWidth) {
                    appendLineRun(line, LineRun{.run = std::move(tokenRun), .width = tokenWidth});
                    lineWidth += tokenWidth;
                    continue;
                }

                if (!line.empty()) {
                    flushLine();
                    if (token.space) {
                        continue;
                    }
                }

                std::size_t index = 0;
                while (index < token.text.size()) {
                    const auto current = static_cast<unsigned char>(token.text[index]);
                    const auto size = std::min(utf8CodepointSize(current), token.text.size() - index);
                    auto piece = token.text.substr(index, size);
                    TextRun pieceRun = run;
                    pieceRun.text = piece;
                    const auto pieceWidth = textWidth(pieceRun, fontSize);
                    if (!line.empty() && lineWidth + pieceWidth > maxWidth) {
                        flushLine();
                    }
                    appendToken(run, std::move(piece));
                    index += size;
                }
            }
        }

        if (!line.empty() || markerPending) {
            flushLine();
        }
    }

    void writeBlock(const PdfBlock& block)
    {
        const auto topSpacing = block.kind == PdfBlockKind::Heading ? 8.0F : 4.0F;
        const auto bottomSpacing = block.kind == PdfBlockKind::Heading ? 8.0F : 6.0F;
        y_ -= topSpacing;

        HPDF_REAL fontSize = 11;
        if (block.kind == PdfBlockKind::Heading) {
            switch (block.heading_level) {
            case 1:
                fontSize = 22;
                break;
            case 2:
                fontSize = 18;
                break;
            case 3:
                fontSize = 15;
                break;
            default:
                fontSize = 13;
                break;
            }
        } else if (block.kind == PdfBlockKind::Code) {
            fontSize = 10;
        }

        auto runs = block.runs;
        if (block.kind == PdfBlockKind::Heading) {
            for (auto& run : runs) {
                run.bold = true;
            }
        }

        const auto lineHeight = fontSize * 1.35F;
        const auto indent = static_cast<HPDF_REAL>(block.list_depth) * listIndent_;
        const auto markerSpace = block.list_marker.empty() ? 0 : markerWidth_;
        const auto x = margin_ + indent;
        const auto maxWidth = page_width_ - (2 * margin_) - indent - markerSpace;
        writeWrappedRuns(runs, fontSize, lineHeight, x, maxWidth, block.list_marker);
        y_ -= bottomSpacing;
    }
};

using CmarkNodePtr = std::unique_ptr<cmark_node, decltype(&cmark_node_free)>;
using CmarkParserPtr = std::unique_ptr<cmark_parser, decltype(&cmark_parser_free)>;

cmark_node* parseMarkdown(std::string_view markdown)
{
    cmark_gfm_core_extensions_ensure_registered();
    CmarkParserPtr parser(cmark_parser_new(CMARK_OPT_DEFAULT), &cmark_parser_free);
    if (!parser) {
        throw std::runtime_error("failed to create Markdown parser");
    }

    for (const auto extensionName : kGfmExtensions) {
        auto* extension = cmark_find_syntax_extension(std::string(extensionName).c_str());
        if (extension == nullptr) {
            continue;
        }
        if (cmark_parser_attach_syntax_extension(parser.get(), extension) == 0) {
            throw std::runtime_error("failed to attach Markdown extension");
        }
    }

    cmark_parser_feed(parser.get(), markdown.data(), markdown.size());
    return cmark_parser_finish(parser.get());
}

std::string markdownToPdf(std::string_view markdown)
{
    CmarkNodePtr root(parseMarkdown(markdown), &cmark_node_free);
    if (!root) {
        throw std::runtime_error("failed to parse Markdown");
    }
    PdfWriter writer(markdownBlocks(root.get()));
    return writer.content();
}

class TextTemplateVariableRenderer final : public DocumentVariableRenderer {
  public:
    explicit TextTemplateVariableRenderer(std::string formatId)
        : format_id_(std::move(formatId))
    {
    }

    [[nodiscard]] std::string_view sourceFormatId() const override
    {
        return format_id_;
    }

    [[nodiscard]] std::string render(std::string_view sourceContent,
                                     const DocumentRenderVariables& variables) const override
    {
        return renderTextTemplate(sourceContent, variables.values_by_name);
    }

  private:
    std::string format_id_;
};

class MarkdownToPdfConverter final : public DocumentConverter {
  public:
    [[nodiscard]] std::string_view sourceFormatId() const override
    {
        return kMarkdownFormat.id;
    }

    [[nodiscard]] std::string_view targetFormatId() const override
    {
        return kPdfFormat.id;
    }

    [[nodiscard]] std::string convert(std::string_view content) const override
    {
        return markdownToPdf(content);
    }
};

}  // namespace

void DocumentFormatRegistry::setDefaultSourceFormat(std::string formatId)
{
    if (formatId.empty()) {
        throw std::runtime_error("default document source format id is required");
    }
    default_source_format_id_ = std::move(formatId);
}

void DocumentFormatRegistry::registerSourceFormat(DocumentSourceFormatInfo format)
{
    if (format.id.empty()) {
        throw std::runtime_error("document source format id is required");
    }
    if (format.implementation_format_id.empty()) {
        format.implementation_format_id = format.id;
    }
    source_formats_.push_back(std::move(format));
}

void DocumentFormatRegistry::registerExportFormat(DocumentExportFormatInfo format)
{
    if (format.id.empty()) {
        throw std::runtime_error("document export format id is required");
    }
    if (format.implementation_format_id.empty()) {
        format.implementation_format_id = format.id;
    }
    export_formats_.push_back(std::move(format));
}

void DocumentFormatRegistry::registerVariableRenderer(
    std::unique_ptr<DocumentVariableRenderer> renderer)
{
    if (!renderer) {
        throw std::runtime_error("document variable renderer is required");
    }
    renderers_.push_back(std::move(renderer));
}

void DocumentFormatRegistry::registerConverter(std::unique_ptr<DocumentConverter> converter)
{
    if (!converter) {
        throw std::runtime_error("document converter is required");
    }
    converters_.push_back(std::move(converter));
}

DocumentFormatCapabilities DocumentFormatRegistry::capabilities() const
{
    return DocumentFormatCapabilities{
        .default_source_format = default_source_format_id_,
        .source_formats = source_formats_,
        .export_formats = export_formats_,
    };
}

const DocumentSourceFormatInfo& DocumentFormatRegistry::sourceFormatById(
    std::string_view formatId) const
{
    for (const auto& format : source_formats_) {
        if (format.id == formatId) {
            return format;
        }
    }
    throw ApplicationError(ApplicationErrorCode::BadRequest, "unsupported source format");
}

const DocumentExportFormatInfo& DocumentFormatRegistry::exportFormatById(
    std::string_view formatId) const
{
    for (const auto& format : export_formats_) {
        if (format.id == formatId) {
            return format;
        }
    }
    throw ApplicationError(ApplicationErrorCode::BadRequest, "unsupported export format");
}

const DocumentSourceFormatInfo& DocumentFormatRegistry::detectSourceFormat(
    std::string_view fileName,
    std::string_view mimeType) const
{
    const auto extension = lowerExtension(fileName);
    if (!extension.empty()) {
        for (const auto& format : source_formats_) {
            for (const auto& candidate : format.extensions) {
                if (extension == lowerAscii(candidate)) {
                    return format;
                }
            }
        }
    }

    const auto mime = mimeTypeWithoutParameters(mimeType);
    if (!mime.empty()) {
        for (const auto& format : source_formats_) {
            for (const auto& candidate : format.mime_types) {
                if (mime == mimeTypeWithoutParameters(candidate)) {
                    return format;
                }
            }
        }
    }

    return sourceFormatById(default_source_format_id_);
}

const DocumentSourceFormatInfo& DocumentFormatRegistry::resolveSourceFormat(
    const std::optional<std::string>& requestedFormat,
    std::string_view fileName,
    std::string_view mimeType) const
{
    if (!requestedFormat || requestedFormat->empty()) {
        return detectSourceFormat(fileName, mimeType);
    }
    return sourceFormatById(*requestedFormat);
}

const DocumentExportFormatInfo& DocumentFormatRegistry::resolveExportFormat(
    const DocumentSourceFormatInfo& sourceFormat,
    const std::optional<std::string>& requestedFormat) const
{
    const auto formatId =
        requestedFormat && !requestedFormat->empty() ? *requestedFormat
                                                     : sourceFormat.default_export_format;
    if (!containsId(sourceFormat.export_format_ids, formatId)) {
        throw ApplicationError(ApplicationErrorCode::BadRequest,
                               "unsupported export format for source format");
    }
    return exportFormatById(formatId);
}

const DocumentVariableRenderer& DocumentFormatRegistry::variableRendererFor(
    const DocumentSourceFormatInfo& sourceFormat) const
{
    for (const auto& renderer : renderers_) {
        if (renderer->sourceFormatId() == sourceFormat.implementation_format_id) {
            return *renderer;
        }
    }
    throw ApplicationError(ApplicationErrorCode::BadRequest,
                           "source format does not support student variables");
}

RenderedDocument DocumentFormatRegistry::convert(const DocumentSourceFormatInfo& sourceFormat,
                                                 const DocumentExportFormatInfo& targetFormat,
                                                 std::string_view content) const
{
    if (sourceFormat.implementation_format_id == targetFormat.implementation_format_id) {
        return RenderedDocument{
            .content = std::string(content),
            .format_id = targetFormat.id,
            .mime_type = targetFormat.mime_type,
            .extension = targetFormat.extension,
        };
    }

    for (const auto& converter : converters_) {
        if (converter->sourceFormatId() == sourceFormat.implementation_format_id &&
            converter->targetFormatId() == targetFormat.implementation_format_id) {
            return RenderedDocument{
                .content = converter->convert(content),
                .format_id = targetFormat.id,
                .mime_type = targetFormat.mime_type,
                .extension = targetFormat.extension,
            };
        }
    }

    throw ApplicationError(ApplicationErrorCode::BadRequest,
                           "unsupported source and export format pair");
}

std::shared_ptr<DocumentFormatRegistry> createDefaultDocumentFormatRegistry()
{
    auto registry = std::make_shared<DocumentFormatRegistry>();

    registry->setDefaultSourceFormat(std::string(kOtherFormat.id));

    registry->registerExportFormat(exportFormat(kOtherFormat,
                                                kTextExtension,
                                                config::plainTextMimeType,
                                                kTextFormat.id));
    registry->registerExportFormat(exportFormat(kTextFormat,
                                                kTextExtension,
                                                config::plainTextMimeType));
    registry->registerExportFormat(exportFormat(kPdfFormat, kPdfExtension, kPdfMimeType));

    registry->registerSourceFormat(sourceFormat(kOtherFormat,
                                                {},
                                                {},
                                                kOtherFormat.id,
                                                strings({kOtherFormat.id}),
                                                kTextFormat.id));
    registry->registerSourceFormat(sourceFormat(kTextFormat,
                                                strings({kTextExtension}),
                                                strings({config::plainTextMimeType}),
                                                kTextFormat.id,
                                                strings({kTextFormat.id})));
    registry->registerSourceFormat(sourceFormat(kMarkdownFormat,
                                                strings(kMarkdownExtensions),
                                                strings({kMarkdownMimeType,
                                                         kMarkdownAlternateMimeType}),
                                                kPdfFormat.id,
                                                strings({kPdfFormat.id})));

    registry->registerVariableRenderer(
        std::make_unique<TextTemplateVariableRenderer>(std::string(kTextFormat.id)));
    registry->registerVariableRenderer(
        std::make_unique<TextTemplateVariableRenderer>(std::string(kMarkdownFormat.id)));
    registry->registerConverter(std::make_unique<MarkdownToPdfConverter>());

    return registry;
}

}  // namespace schoolmanager::application
