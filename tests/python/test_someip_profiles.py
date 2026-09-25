"""Keep local, home, and workplace vSomeIP network profiles distinct."""

import json
from pathlib import Path


CONFIG = Path(__file__).resolve().parents[2] / "config" / "someip"


def profile(name):
    return json.loads((CONFIG / name).read_text())


def without_address(config):
    return {key: value for key, value in config.items() if key != "unicast"}


def test_local_profiles_remain_loopback():
    provider = profile("provider.json")
    client = profile("client.json")
    assert provider["unicast"] == client["unicast"] == "127.0.0.1"
    assert provider["routing"] == "vehicle-provider"
    assert client["routing"] == "vehicle-provider"


def test_home_and_work_profiles_differ_only_in_ip():
    home_provider = profile("provider_pi.json")
    work_provider = profile("provider_office.json")
    home_client = profile("client_pc.json")
    work_client = profile("client_office.json")
    assert home_provider["unicast"] == "192.168.50.2"
    assert home_client["unicast"] == "192.168.50.1"
    assert work_provider["unicast"] == "192.168.137.69"
    assert work_client["unicast"] == "192.168.137.1"
    assert without_address(home_provider) == without_address(work_provider)
    assert without_address(home_client) == without_address(work_client)


def test_multi_host_service_and_event_contract():
    provider = profile("provider_office.json")
    client = profile("client_office.json")
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
    assert {event["event"] for event in service["events"]} == {"0x8001", "0x8002"}
    assert service["eventgroups"] == [
        {"eventgroup": "0x0001", "events": ["0x8001"]},
        {"eventgroup": "0x0002", "events": ["0x8002"]},
    ]
    for config in (provider, client):
        discovery = config["service-discovery"]
        assert discovery["enable"] == "true"
        assert (discovery["multicast"], discovery["port"], discovery["protocol"]) == (
            "224.244.224.245", "30490", "udp"
        )
