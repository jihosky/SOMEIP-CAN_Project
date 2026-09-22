from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))

from virtual_ecu.__main__ import (
    COOLANT_TEMPERATURE_C,
    DEFINITION,
    ENGINE_RPM,
    VEHICLE_SPEED_KPH,
    make_message,
)


def test_milestone_frame_is_standard_and_deterministic():
    message = make_message()

    assert message.arbitration_id == DEFINITION["can_id"] == 0x100
    assert message.is_extended_id is False
    assert message.dlc == 8
    assert message.data == bytes.fromhex("39 30 C4 09 7D 00 00 00")
    assert VEHICLE_SPEED_KPH == 123.45
    assert ENGINE_RPM == 2500
    assert COOLANT_TEMPERATURE_C == 85
