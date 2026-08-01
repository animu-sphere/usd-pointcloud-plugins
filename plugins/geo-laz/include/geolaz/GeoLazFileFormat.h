#pragma once

#include "pxr/base/tf/staticTokens.h"
#include <pxr/usd/sdf/fileFormat.h>

PXR_NAMESPACE_OPEN_SCOPE

#define GEOLAZ_FILE_FORMAT_TOKENS \
    ((Id, "laz")) \
    ((Version, "1.0")) \
    ((Target, "usd")) \
    ((Extension, "laz"))

TF_DECLARE_PUBLIC_TOKENS(GeoLazFileFormatTokens, GEOLAZ_FILE_FORMAT_TOKENS);

class GeoLazFileFormat final : public SdfFileFormat {
public:
    bool CanRead(const std::string& file) const override;
    bool Read(SdfLayer* layer,
              const std::string& resolvedPath,
              bool metadataOnly) const override;
    bool WriteToString(const SdfLayer& layer,
                       std::string* str,
                       const std::string& comment = std::string()) const override;

    GeoLazFileFormat();
    ~GeoLazFileFormat() override;

protected:
    SDF_FILE_FORMAT_FACTORY_ACCESS;
};

PXR_NAMESPACE_CLOSE_SCOPE
