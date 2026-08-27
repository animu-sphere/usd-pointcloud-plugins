"""Component-owned packaged LAS/LAZ/PLY/COPC discovery and read probe.

SPDX-License-Identifier: Apache-2.0
The fixture argument is composition-owned; other fixtures ship in the bundles.
"""
import argparse
import json
from pathlib import Path


def main():
    from pxr import Plug, Sdf, Usd, UsdGeom

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--copc-fixture', type=Path, required=True)
    args = parser.parse_args()
    checks = []
    for extension, stem in [('las', 'Las'), ('laz', 'Laz'), ('ply', 'Ply'), ('copc', 'Copc')]:
        plugin = Plug.Registry().GetPluginWithName(f'UsdGeo{stem}FileFormat')
        if not plugin or not plugin.Load() or not Sdf.FileFormat.FindByExtension(extension):
            raise RuntimeError(f'{extension}: packaged file format did not load')
        fixture = (args.copc_fixture if extension == 'copc' else
                   args.prefix / f'bundles/pointcloud-{extension}/tests/fixtures/conformance.{extension}')
        layer = Sdf.Layer.FindOrOpen(str(fixture), {'epsg': '4978'} if extension == 'ply' else {})
        stage = Usd.Stage.Open(layer) if layer else None
        if not stage:
            raise RuntimeError(f'{extension}: fixture did not open')
        point_prims = [UsdGeom.Points(prim) for prim in stage.Traverse() if prim.IsA(UsdGeom.Points)]
        count = sum(len(prim.GetPointsAttr().Get() or []) for prim in point_prims)
        if count < 1:
            raise RuntimeError(f'{extension}: fixture authored no points')
        checks.append({'format': extension, 'status': 'passed', 'pointCount': count,
                       'pluginPath': plugin.path})
    # Actual OpenUSD schema discovery, independent of plugInfo path checks.
    if not Usd.SchemaRegistry().FindConcretePrimDefinition('Points'):
        raise RuntimeError('UsdGeom Points schema is not registered')
    print(json.dumps({'probe': 'pointcloud-plugins', 'status': 'passed',
                      'schema': 'UsdGeomPoints', 'checks': checks}))


if __name__ == '__main__':
    main()
