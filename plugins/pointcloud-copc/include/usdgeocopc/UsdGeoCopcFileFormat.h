#pragma once

#include "pxr/base/tf/staticTokens.h"
#include <pxr/usd/pcp/dynamicFileFormatInterface.h>
#include <pxr/usd/sdf/fileFormat.h>

PXR_NAMESPACE_OPEN_SCOPE

#define USDGEOCOPC_FILE_FORMAT_TOKENS \
    ((Id, "copc")) \
    ((Version, "1.0")) \
    ((Target, "usd")) \
    ((Extension, "copc"))

TF_DECLARE_PUBLIC_TOKENS(UsdGeoCopcFileFormatTokens, USDGEOCOPC_FILE_FORMAT_TOKENS);

class UsdGeoCopcFileFormat final : public SdfFileFormat,
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

    UsdGeoCopcFileFormat();
    ~UsdGeoCopcFileFormat() override;

protected:
    SDF_FILE_FORMAT_FACTORY_ACCESS;
};

PXR_NAMESPACE_CLOSE_SCOPE
