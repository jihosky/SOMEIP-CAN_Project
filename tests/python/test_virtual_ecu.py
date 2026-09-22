from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))

from virtual_ecu.__main__ import TEST_CAN_ID, TEST_PAYLOAD, make_message


def test_milestone_frame_is_standard_and_deterministic():
    message = make_message()

    assert message.arbitration_id == TEST_CAN_ID == 0x100
    assert message.is_extended_id is False
    assert message.dlc == 8
    assert message.data == TEST_PAYLOAD == bytes(range(8))
