#pragma once

#include "pxr/base/tf/staticTokens.h"
#include <pxr/usd/pcp/dynamicFileFormatInterface.h>
#include <pxr/usd/sdf/fileFormat.h>

PXR_NAMESPACE_OPEN_SCOPE

#define USDGEOLAZ_FILE_FORMAT_TOKENS \
    ((Id, "laz")) \
    ((Version, "1.0")) \
    ((Target, "usd")) \
    ((Extension, "laz"))

TF_DECLARE_PUBLIC_TOKENS(UsdGeoLazFileFormatTokens, USDGEOLAZ_FILE_FORMAT_TOKENS);

class UsdGeoLazFileFormat final : public SdfFileFormat,
                                  public PcpDynamicFileFormatInterface {
public:
    bool CanRead(const std::string& file) const override;
    bool Read(SdfLayer* layer,
              const std::string& resolvedPath,
              bool metadataOnly) const override;
    bool WriteToString(const SdfLayer& layer,
                       std::string* str,
                       const std::string& comment = std::string()) const override;
    void ComposeFieldsForFileFormatArguments(
        const std::string& assetPath,
        const PcpDynamicFileFormatContext& context,
        FileFormatArguments* args,
        VtValue* dependencyContextData) const override;
    bool CanFieldChangeAffectFileFormatArguments(
        const TfToken& field,
        const VtValue& oldValue,
        const VtValue& newValue,
        const VtValue& dependencyContextData) const override;

    UsdGeoLazFileFormat();
    ~UsdGeoLazFileFormat() override;

protected:
    SDF_FILE_FORMAT_FACTORY_ACCESS;
};

PXR_NAMESPACE_CLOSE_SCOPE
