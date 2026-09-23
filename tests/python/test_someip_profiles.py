"""Check that local and two-host vSomeIP profiles stay distinct."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CONFIG = ROOT / "config" / "someip"


def profile(name):
    return json.loads((CONFIG / name).read_text())


def test_local_profiles_remain_loopback():
    provider = profile("provider.json")
    client = profile("client.json")
    assert provider["unicast"] == client["unicast"] == "127.0.0.1"
    assert provider["routing"] == "vehicle-provider"
    assert client["routing"] == "vehicle-provider"


def test_multi_host_profiles_match_service_and_network_contract():
    provider = profile("provider_pi.json")
    client = profile("client_pc.json")
    assert (provider["unicast"], client["unicast"]) == (
        "192.168.50.2", "192.168.50.1"
    )
    assert provider["device"] == client["device"] == "eth0"
    assert provider["netmask"] == client["netmask"] == "255.255.255.0"
    assert provider["routing"] == "vehicle-provider"
    assert client["routing"] == "vehicle-client"
    assert provider["applications"] == [{"name": "vehicle-provider", "id": "0x6301"}]
    assert client["applications"] == [{"name": "vehicle-client", "id": "0x6302"}]
    service = provider["services"][0]
    assert (service["service"], service["instance"], service["reliable"]) == (
        "0x6301", "0x0001", {"port": "30540", "enable-magic-cookies": "false"}
    )
    assert service["eventgroups"][0] == {
        "eventgroup": "0x0001", "events": ["0x8001"]
    }
    for config in (provider, client):
        discovery = config["service-discovery"]
        assert discovery["enable"] == "true"
        assert (discovery["multicast"], discovery["port"], discovery["protocol"]) == (
            "224.244.224.245", "30490", "udp"
        )
