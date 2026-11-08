#!/usr/bin/env python3
"""Generate minimal Papyrus PEX files for Artisans of Skyrim.

The scripts in this package are intentionally tiny:

* ArtisansOfSkyrim.psc declares global native functions implemented by the DLL.
* AOS_TIF_*.psc fragments cast the dialogue speaker to Actor and call one native.
* AOS_MCM.psc forwards MCM Helper setting changes to one native.

The tool emits PEX 3.2 bytecode for these fixed bridge signatures without
requiring Creation Kit's PapyrusCompiler. It is not a general Papyrus compiler.
"""

from __future__ import annotations

import struct
import time
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "dist" / "Data" / "Scripts"
SOURCE = ROOT / "papyrus" / "Source"


def u8(value: int) -> bytes:
    return struct.pack(">B", value)


def u16(value: int) -> bytes:
    return struct.pack(">H", value)


def u32(value: int) -> bytes:
    return struct.pack(">I", value)


def u64(value: int) -> bytes:
    return struct.pack(">Q", value)


def pstr(value: str) -> bytes:
    raw = value.encode("utf-8")
    return u16(len(raw)) + raw


@dataclass(frozen=True)
class Function:
    name: str
    return_type: str = "None"
    doc: str = ""
    flags: int = 0
    params: tuple[tuple[str, str], ...] = ()
    locals: tuple[tuple[str, str], ...] = ()
    instructions: tuple[bytes, ...] = ()


class PexBuilder:
    def __init__(self, script_name: str, source_name: str, parent: str = "") -> None:
        self.script_name = script_name
        self.source_name = source_name
        self.parent = parent
        self.strings: list[str] = []
        self.index: dict[str, int] = {}

    def idx(self, value: str) -> int:
        if value not in self.index:
            self.index[value] = len(self.strings)
            self.strings.append(value)
        return self.index[value]

    def id(self, value: str) -> bytes:
        return u8(1) + u16(self.idx(value))

    def intv(self, value: int) -> bytes:
        return u8(3) + u32(value)

    def function_bytes(self, func: Function) -> bytes:
        data = (
            u16(self.idx(func.name))
            + u16(self.idx(func.return_type))
            + u16(self.idx(func.doc))
            + u32(0)
            + u8(func.flags)
            + u16(len(func.params))
        )
        for name, typ in func.params:
            data += u16(self.idx(name)) + u16(self.idx(typ))
        data += u16(len(func.locals))
        for name, typ in func.locals:
            data += u16(self.idx(name)) + u16(self.idx(typ))
        data += u16(len(func.instructions))
        for instruction in func.instructions:
            data += instruction
        return data

    def goto_state_function(self) -> Function:
        return Function(
            name="GotoState",
            doc="Function that switches this object to the specified state",
            params=(("newState", "String"),),
            locals=(("::NoneVar", "None"),),
            instructions=(
                self.call_method("onEndState", "self", "::NoneVar", ()),
                self.assign("::State", "newState"),
                self.call_method("onBeginState", "self", "::NoneVar", ()),
            ),
        )

    def get_state_function(self) -> Function:
        return Function(
            name="GetState",
            return_type="String",
            doc="Function that returns the current state",
            instructions=(u8(0x1A) + self.id("::State"),),
        )

    def assign(self, target: str, source: str) -> bytes:
        return u8(0x0D) + self.id(target) + self.id(source)

    def cast(self, target: str, source: str) -> bytes:
        return u8(0x0E) + self.id(target) + self.id(source)

    def call_static(
        self,
        script_name: str,
        function_name: str,
        destination: str,
        args: tuple[str, ...],
    ) -> bytes:
        data = (
            u8(0x19)
            + self.id(script_name)
            + self.id(function_name)
            + self.id(destination)
            + self.intv(len(args))
        )
        for arg in args:
            data += self.id(arg)
        return data

    def call_method(
        self,
        function_name: str,
        target: str,
        destination: str,
        args: tuple[str, ...],
    ) -> bytes:
        data = (
            u8(0x17)
            + self.id(function_name)
            + self.id(target)
            + self.id(destination)
            + self.intv(len(args))
        )
        for arg in args:
            data += self.id(arg)
        return data

    def build(self, functions: tuple[Function, ...], debug_lines: dict[str, tuple[int, ...]]) -> bytes:
        # Pre-register common strings in compiler-like order for stable output.
        for value in (
            self.script_name,
            "",
            "GetState",
            "GotoState",
            "hidden",
            "conditional",
            self.parent,
            "None",
        ):
            self.idx(value)
        for func in functions:
            self.idx(func.name)
            self.idx(func.return_type)
            self.idx(func.doc)
            for name, typ in func.params + func.locals:
                self.idx(name)
                self.idx(typ)

        object_body = (
            u16(self.idx(self.parent))
            + u16(self.idx(""))
            + u32(1)
            + u16(self.idx(""))
            + u16(0)
            + u16(0)
            + u16(1)
            + u16(self.idx(""))
            + u16(len(functions))
        )
        for func in functions:
            object_body += self.function_bytes(func)
        object_record = u16(self.idx(self.script_name)) + u32(len(object_body) + 6) + object_body

        now = int(time.time())
        header = (
            u32(0xFA57C0DE)
            + u8(3)
            + u8(2)
            + u16(1)
            + u64(now)
            + pstr(self.source_name)
            + pstr("Artisans of Skyrim")
            + pstr("Artisans of Skyrim")
        )

        string_table = u16(len(self.strings)) + b"".join(pstr(value) for value in self.strings)

        debug = u8(1) + u64(now) + u16(len(functions))
        for func in functions:
            lines = debug_lines.get(func.name, ())
            debug += (
                u16(self.idx(self.script_name))
                + u16(self.idx(""))
                + u16(self.idx(func.name))
                + u8(0)
                + u16(len(lines))
            )
            for line in lines:
                debug += u16(line)

        user_flags = u16(2) + u16(self.idx("hidden")) + u8(0) + u16(self.idx("conditional")) + u8(1)
        objects = u16(1) + object_record
        return header + string_table + debug + user_flags + objects


def fragment(script_name: str, native_function: str, speaker_param: str) -> bytes:
    builder = PexBuilder(script_name, f"{script_name}.psc", parent="TopicInfo")
    fragment_function = Function(
        name="Fragment_0",
        params=(("akSpeakerRef", "ObjectReference"),),
        locals=(("::temp0", "actor"), ("::NoneVar", "None"), ("akSpeaker", "actor")),
        instructions=(
            builder.cast("::temp0", "akSpeakerRef"),
            builder.assign("akSpeaker", "::temp0"),
            builder.call_static("ArtisansOfSkyrim", native_function, "::NoneVar", ("akSpeaker",)),
        ),
    )
    functions = (
        fragment_function,
        builder.goto_state_function(),
        builder.get_state_function(),
    )
    return builder.build(functions, {"Fragment_0": (7, 7, 9), "GotoState": (), "GetState": ()})


def native_bridge() -> bytes:
    builder = PexBuilder("ArtisansOfSkyrim", "ArtisansOfSkyrim.psc")
    native_functions = (
        Function("OpenSmithCraftMenu", flags=0x03, params=(("akSmith", "Actor"),)),
        Function("OpenSmithImproveMenu", flags=0x03, params=(("akSmith", "Actor"),)),
        Function("OpenEnchantMenu", flags=0x03, params=(("akEnchanter", "Actor"),)),
        Function("OpenAlchemyMenu", flags=0x03, params=(("akAlchemist", "Actor"),)),
        Function("CollectSmithOrders", flags=0x03, params=(("akSmith", "Actor"),)),
        Function(
            "ApplyMcmSetting",
            flags=0x03,
            params=(("asSettingId", "String"), ("afValue", "Float"), ("aiValue", "Int")),
        ),
        Function("onBeginState"),
        builder.goto_state_function(),
        builder.get_state_function(),
        Function("onEndState"),
    )
    return builder.build(native_functions, {func.name: () for func in native_functions})


def mcm_script() -> bytes:
    builder = PexBuilder("AOS_MCM", "AOS_MCM.psc", parent="MCM_ConfigBase")
    on_setting_change = Function(
        name="OnSettingChange",
        params=(("a_ID", "String"),),
        locals=(("::temp0", "float"), ("::temp1", "Int"), ("::NoneVar", "None")),
        instructions=(
            builder.call_method("GetModSettingFloat", "self", "::temp0", ("a_ID",)),
            builder.call_method("GetModSettingInt", "self", "::temp1", ("a_ID",)),
            builder.call_static(
                "ArtisansOfSkyrim",
                "ApplyMcmSetting",
                "::NoneVar",
                ("a_ID", "::temp0", "::temp1"),
            ),
        ),
    )
    functions = (
        on_setting_change,
        builder.goto_state_function(),
        builder.get_state_function(),
    )
    return builder.build(
        functions,
        {"OnSettingChange": (8, 8, 8), "GotoState": (), "GetState": ()},
    )


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    generated = {
        "ArtisansOfSkyrim.pex": native_bridge(),
        "AOS_TIF_SmithCraft.pex": fragment("AOS_TIF_SmithCraft", "OpenSmithCraftMenu", "akSmith"),
        "AOS_TIF_SmithImprove.pex": fragment("AOS_TIF_SmithImprove", "OpenSmithImproveMenu", "akSmith"),
        "AOS_TIF_Enchant.pex": fragment("AOS_TIF_Enchant", "OpenEnchantMenu", "akEnchanter"),
        "AOS_TIF_Alchemy.pex": fragment("AOS_TIF_Alchemy", "OpenAlchemyMenu", "akAlchemist"),
        "AOS_TIF_CollectOrders.pex": fragment(
            "AOS_TIF_CollectOrders",
            "CollectSmithOrders",
            "akSmith",
        ),
        "AOS_MCM.pex": mcm_script(),
    }
    for name, data in generated.items():
        (OUT / name).write_bytes(data)
        print(f"Generated {OUT / name} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
