"""Tier 2 resolver interoperability harness.

Composes an external OpenUSD resolver, a local range-serving HTTP origin, and a
COPC fixture, then records what a remote read actually costs. It is a
measurement driver: it links nothing, and this repository keeps no build edge to
any resolver implementation.

Run it inside an activated OpenStrata runtime, e.g.

    python tools/tier2_resolver_integration.py \
        --fixture build/real-data-source/autzen-classified.copc.laz \
        --resolver-resources <resolver>/plugin/resources/httpResolver \
        --copc-resources plugins/pointcloud-copc/plugin/resources/pointcloud-copc \
        --output build/tier2/record.json

Each scenario runs in a fresh interpreter against a fresh origin, so no
in-process resolver state and no request log is carried between rows and
`bytesFetched` is that scenario's cost alone.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROUTE = "/fixture.copc"


# --------------------------------------------------------------------------
# Scenario bodies. These run in the child interpreter, one per invocation.
# --------------------------------------------------------------------------

def _summarize_stage(layer) -> dict:
    from pxr import Sdf, Usd, UsdGeom

    stage = Usd.Stage.Open(layer)
    points = UsdGeom.Points.Get(stage, Sdf.Path("/PointCloud"))
    result = {"prims": sorted(prim.name for prim in layer.rootPrims)}
    if not points:
        return result
    positions = points.GetPointsAttr().Get()
    result["pointCount"] = 0 if positions is None else len(positions)
    digest = hashlib.sha256()
    if positions is not None:
        for position in positions:
            digest.update(
                ("%.6f,%.6f,%.6f;" % (position[0], position[1], position[2]))
                .encode("ascii"))
    result["pointDigest"] = digest.hexdigest()
    extent = points.GetExtentAttr().Get()
    if extent is not None:
        result["extent"] = [[float(value) for value in corner]
                            for corner in extent]
    return result


def run_scenario(name: str, target: str) -> dict:
    from pxr import Ar, Sdf

    record = {"scenario": name, "target": target}
    resolver = Ar.GetResolver()
    if target.startswith("http"):
        resolved = resolver.Resolve(target)
        info = resolver.GetAssetInfo(target, resolved)
        identifier = resolved.GetPathString()
        # The token is opaque to this repository and is never recorded. Only
        # whether one exists, and a digest that lets a later revision be seen
        # to differ without the value itself reaching the record.
        record["hasValidationToken"] = bool(info.version)
        record["validationTokenDigest"] = (
            hashlib.sha256(info.version.encode("utf-8")).hexdigest()[:16]
            if info.version else "")
        record["identityClass"] = (
            "stable" if identifier and info.version
            else "unstable" if identifier else "unavailable")

    start = time.monotonic()
    if name == "metadata":
        layer = Sdf.Layer.OpenAsAnonymous(target, metadataOnly=True)
        record["opened"] = bool(layer)
        if layer:
            record["prims"] = sorted(prim.name for prim in layer.rootPrims)
    else:
        layer = Sdf.Layer.FindOrOpen(target)
        record["opened"] = bool(layer)
        if layer:
            record.update(_summarize_stage(layer))
    record["elapsedSeconds"] = round(time.monotonic() - start, 4)
    return record


# --------------------------------------------------------------------------
# Driver.
# --------------------------------------------------------------------------

def reserve_port() -> int:
    """One port for the whole run.

    Every revision has to be served at the *same* URL, or the run is not three
    revisions of one identifier - it is three unrelated assets, and the property
    being demonstrated (equal identifier, different validator) is not being
    demonstrated at all. Origins run one at a time, so a single reserved port is
    enough.
    """
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


class Origin:
    """The loopback fixture origin, plus the request log it writes."""

    def __init__(self, server: Path, fixture: Path, log: Path, validator: str,
                 port: int):
        self.log = log
        self.process = subprocess.Popen(
            [sys.executable, str(server), "--file", str(fixture),
             "--route", ROUTE, "--log", str(log), "--port", str(port),
             "--validator", validator],
            stdout=subprocess.PIPE, text=True)
        try:
            self.port = int(self._line().split("=", 1)[1])
            self.size = int(self._line().split("=", 1)[1])
        except BaseException:
            # The handshake can fail after the process exists; leaving it
            # running would hold the reserved port for the rest of the run.
            self.close()
            raise

    def _line(self) -> str:
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError("fixture origin did not start")
        return line.strip()

    @property
    def url(self) -> str:
        return "http://127.0.0.1:%d%s" % (self.port, ROUTE)

    def stats(self) -> dict:
        with open(self.log, encoding="utf-8") as handle:
            data = json.load(handle)
        requests = data["requests"]
        fetched = sum(entry["bytesSent"] for entry in requests)
        source = data["sourceBytes"]
        return {"requests": len(requests),
                "rangeRequests": sum(1 for entry in requests
                                     if entry["range"]),
                "bytesFetched": fetched,
                "sourceBytes": source,
                "selectivity": round(fetched / source, 6) if source else 0.0}

    def close(self) -> None:
        self.process.kill()
        self.process.wait()


# The four generated-cache decision codes the COPC plugin emits. A scenario
# records which ones OpenUSD reported, because "reuse was disabled" is a claim
# about a diagnostic, not about a return value.
DECISION_CODES = ("COPC009", "COPC010", "COPC011", "COPC012")


def child(script: Path, environment: dict, name: str, target: str,
          cache_root=None) -> dict:
    child_environment = dict(environment)
    if cache_root is not None:
        cache_root.mkdir(parents=True, exist_ok=True)
        child_environment["USDGEO_CACHE_ROOT"] = str(cache_root)
    else:
        child_environment.pop("USDGEO_CACHE_ROOT", None)
    completed = subprocess.run(
        [sys.executable, str(script), "--run-scenario", name,
         "--target", target],
        capture_output=True, text=True, env=child_environment)
    if completed.returncode != 0:
        raise RuntimeError("scenario %s failed:\n%s\n%s"
                           % (name, completed.stdout, completed.stderr))
    record = json.loads(completed.stdout.strip().splitlines()[-1])
    record["cacheRootConfigured"] = cache_root is not None
    record["decisionCodes"] = [code for code in DECISION_CODES
                               if code in completed.stderr]
    record["decisionDiagnostics"] = [
        line.strip() for line in completed.stderr.splitlines()
        if any(code in line for code in DECISION_CODES)
    ]
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-scenario")
    parser.add_argument("--target")
    parser.add_argument("--fixture")
    parser.add_argument("--resolver-resources")
    parser.add_argument("--copc-resources")
    parser.add_argument("--output")
    arguments = parser.parse_args()

    if arguments.run_scenario:
        print(json.dumps(run_scenario(arguments.run_scenario,
                                      arguments.target)))
        return 0

    for required in ("fixture", "resolver_resources", "copc_resources",
                     "output"):
        if not getattr(arguments, required):
            parser.error("--%s is required" % required.replace("_", "-"))

    script = Path(__file__).resolve()
    server = script.parent / "tier2_fixture_server.py"
    fixture = Path(arguments.fixture).resolve()
    output = Path(arguments.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    environment = dict(os.environ)
    environment["PXR_PLUGINPATH_NAME"] = os.pathsep.join(
        entry for entry in
        [str(Path(arguments.copc_resources).resolve()),
         str(Path(arguments.resolver_resources).resolve()),
         environment.get("PXR_PLUGINPATH_NAME", "")] if entry)

    with open(fixture, "rb") as handle:
        fixture_bytes = handle.read()
    record = {
        "fixture": {
            "name": fixture.name,
            "sizeBytes": len(fixture_bytes),
            "sha256": hashlib.sha256(fixture_bytes).hexdigest(),
        },
        "scenarios": [],
    }

    workspace = Path(tempfile.mkdtemp(prefix="tier2-resolver-"))
    try:
        local = workspace / "local.copc"
        shutil.copyfile(fixture, local)

        # Local baseline: the same bytes through the same FileFormat with no
        # resolver in the path. Its authored output is the oracle the remote
        # rows are compared against.
        record["scenarios"].append(
            child(script, environment, "full-local", str(local)))

        # Three revisions of one identifier, served at one reserved port so the
        # identifier really is one identifier. A and B differ only in the
        # validator, so a changed validation identity is visible without the
        # bytes changing - identifier equality is not content equality. W serves
        # a weak validator, which a resolver must not publish as a stable
        # identity, and is how the conservative fallback is exercised against a
        # real resolver rather than a test double.
        port = reserve_port()
        # One cache root for the whole run: separate roots per scenario would
        # make every lookup a first lookup and hide any relationship between
        # revisions.
        cache_root = workspace / "cache"
        revisions = (("A", '"revision-a"'),
                     ("B", '"revision-b"'),
                     ("W", 'W/"revision-w"'))
        identifiers = set()
        for revision, validator in revisions:
            for name in ("metadata", "full", "full-repeat"):
                log = workspace / ("origin-%s-%s.json" % (revision, name))
                origin = Origin(server, fixture, log, validator, port)
                try:
                    entry = child(script, environment, name, origin.url,
                                  cache_root)
                    entry["revision"] = revision
                    entry["validatorStrength"] = (
                        "weak" if validator.startswith("W/") else "strong")
                    entry["origin"] = origin.stats()
                    identifiers.add(origin.url)
                    record["scenarios"].append(entry)
                finally:
                    origin.close()
        if len(identifiers) != 1:
            raise RuntimeError(
                "revisions were served at %d identifiers, not one: %s"
                % (len(identifiers), sorted(identifiers)))
        record["identifier"] = identifiers.pop()
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    with open(output, "w", encoding="utf-8") as handle:
        json.dump(record, handle, indent=2)
    print(json.dumps(record, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

