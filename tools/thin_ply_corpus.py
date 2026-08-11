"""Create deterministic, point-only PLY corpus subsets."""

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path


SCALAR_FORMATS = {
    "char": "b",
    "int8": "b",
    "uchar": "B",
    "uint8": "B",
    "short": "h",
    "int16": "h",
    "ushort": "H",
    "uint16": "H",
    "int": "i",
    "int32": "i",
    "uint": "I",
    "uint32": "I",
    "float": "f",
    "float32": "f",
    "double": "d",
    "float64": "d",
}


def parse_header(stream):
    lines = []
    while True:
        raw_line = stream.readline()
        if not raw_line:
            raise ValueError("PLY header is truncated")
        line = raw_line.decode("ascii").strip()
        lines.append(line)
        if line == "end_header":
            break

    if not lines or lines[0] != "ply":
        raise ValueError("input is not a PLY file")
    format_name = lines[1].split()[1]
    if format_name not in {"ascii", "binary_little_endian"}:
        raise ValueError("only ASCII and binary little-endian PLY are supported")

    vertex_count = None
    vertex_properties = []
    current_element = None
    for line in lines[2:-1]:
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "element":
            current_element = fields[1]
            if current_element == "vertex":
                vertex_count = int(fields[2])
        elif fields[0] == "property" and current_element == "vertex":
            if fields[1] == "list":
                raise ValueError("vertex list properties are not supported")
            property_type = fields[1]
            if property_type not in SCALAR_FORMATS:
                raise ValueError(f"unsupported vertex property type: {property_type}")
            vertex_properties.append((property_type, fields[2]))

    if vertex_count is None or not vertex_properties:
        raise ValueError("PLY has no scalar vertex element")
    return format_name, vertex_count, vertex_properties


def selected_indices(point_count: int, target_count: int) -> set[int]:
    if target_count < 2:
        raise ValueError("target count must be at least 2")
    if target_count > point_count:
        raise ValueError("target count exceeds source point count")
    return {
        (index * (point_count - 1)) // (target_count - 1)
        for index in range(target_count)
    }


def read_ascii_vertices(stream, point_count, properties, selected):
    rows = []
    for point_index in range(point_count):
        line = stream.readline()
        if not line:
            raise ValueError("ASCII vertex data is truncated")
        if point_index not in selected:
            continue
        fields = line.decode("ascii").split()
        if len(fields) != len(properties):
            raise ValueError("ASCII vertex property count does not match header")
        rows.append([parse_value(value, property_type)
                     for value, (property_type, _) in zip(fields, properties)])
    return rows


def read_binary_vertices(stream, point_count, properties, selected):
    format_string = "<" + "".join(
        SCALAR_FORMATS[property_type] for property_type, _ in properties)
    record_size = struct.calcsize(format_string)
    rows = []
    for point_index in range(point_count):
        record = stream.read(record_size)
        if len(record) != record_size:
            raise ValueError("binary vertex data is truncated")
        if point_index in selected:
            rows.append(list(struct.unpack(format_string, record)))
    return rows


def parse_value(value: str, property_type: str):
    if property_type in {"float", "float32", "double", "float64"}:
        return float(value)
    return int(value)


def format_value(value, property_type: str) -> str:
    if property_type in {"float", "float32", "double", "float64"}:
        if not math.isfinite(value):
            raise ValueError("non-finite vertex value is not supported")
        return format(value, ".17g")
    return str(value)


def thin_ply(source: Path, destination: Path, target_count: int,
             renamed_properties: dict[str, str]) -> None:
    with source.open("rb") as stream:
        format_name, point_count, properties = parse_header(stream)
        selected = selected_indices(point_count, target_count)
        if format_name == "ascii":
            rows = read_ascii_vertices(stream, point_count, properties, selected)
        else:
            rows = read_binary_vertices(stream, point_count, properties, selected)

    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("w", encoding="ascii", newline="\n") as output:
        output.write("ply\nformat ascii 1.0\n")
        output.write("comment deterministic evenly spaced point subset\n")
        output.write(f"element vertex {len(rows)}\n")
        for property_type, property_name in properties:
            output_name = renamed_properties.get(property_name, property_name)
            output.write(f"property {property_type} {output_name}\n")
        output.write("end_header\n")
        for row in rows:
            output.write(" ".join(
                format_value(value, property_type)
                for value, (property_type, _) in zip(row, properties)
            ))
            output.write("\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--target-count", type=int, required=True)
    parser.add_argument(
        "--rename-property", action="append", default=[], metavar="FROM=TO",
        help="rename a scalar vertex property in the output (repeatable)")
    arguments = parser.parse_args()
    renamed_properties = {}
    for mapping in arguments.rename_property:
        source_name, separator, destination_name = mapping.partition("=")
        if not separator or not source_name or not destination_name:
            parser.error(f"invalid property rename: {mapping}")
        renamed_properties[source_name] = destination_name
    thin_ply(arguments.source, arguments.destination, arguments.target_count,
             renamed_properties)


if __name__ == "__main__":
    main()