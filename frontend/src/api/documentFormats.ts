import type {
  DocumentExportFormat,
  DocumentSourceFormat,
  FileTemplateCapabilities,
} from './types';

export type DocumentFileMatchInput = {
  name: string;
  mime_type: string;
};

const lowerMimeType = (mimeType: string) => mimeType.split(';', 1)[0].trim().toLowerCase();

const fileExtension = (fileName: string) => {
  const dot = fileName.lastIndexOf('.');
  return dot >= 0 ? fileName.slice(dot).toLowerCase() : '';
};

export const sourceFormatById = (
  capabilities: FileTemplateCapabilities,
  formatId: string,
): DocumentSourceFormat | null =>
  capabilities.source_formats.find((format) => format.id === formatId) ?? null;

export const exportFormatById = (
  capabilities: FileTemplateCapabilities,
  formatId: string,
): DocumentExportFormat | null =>
  capabilities.export_formats.find((format) => format.id === formatId) ?? null;

export const defaultSourceFormat = (
  capabilities: FileTemplateCapabilities,
): DocumentSourceFormat | null =>
  sourceFormatById(capabilities, capabilities.default_source_format) ??
  capabilities.source_formats[0] ??
  null;

export const detectDocumentSourceFormat = (
  capabilities: FileTemplateCapabilities,
  file: DocumentFileMatchInput | null,
): DocumentSourceFormat | null => {
  if (!file) {
    return defaultSourceFormat(capabilities);
  }

  const extension = fileExtension(file.name);
  const byExtension = extension
    ? capabilities.source_formats.find((format) =>
        format.extensions.some((candidate) => candidate.toLowerCase() === extension),
      )
    : null;
  if (byExtension) {
    return byExtension;
  }

  const mimeType = lowerMimeType(file.mime_type);
  return (
    capabilities.source_formats.find((format) =>
      format.mime_types.some((candidate) => lowerMimeType(candidate) === mimeType),
    ) ?? defaultSourceFormat(capabilities)
  );
};

export const exportFormatsForSource = (
  capabilities: FileTemplateCapabilities,
  sourceFormat: DocumentSourceFormat | null,
): DocumentExportFormat[] =>
  sourceFormat
    ? sourceFormat.export_formats
        .map((formatId) => exportFormatById(capabilities, formatId))
        .filter((format): format is DocumentExportFormat => Boolean(format))
    : [];

export const defaultExportFormatForSource = (
  capabilities: FileTemplateCapabilities,
  sourceFormat: DocumentSourceFormat | null,
): DocumentExportFormat | null =>
  sourceFormat ? exportFormatById(capabilities, sourceFormat.default_export_format) : null;
