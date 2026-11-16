#include "../src/configs/configs.hpp"
#include <cassert>
#include <fstream>

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::filesystem::path directory(argv[1]);
    const std::string payload = R"({"modules":[]})";
    configs::initialize(directory);
    assert(configs::get_snapshot().success && configs::get_snapshot().entries.empty());
    configs::publish(0, "Test profile", "Unicode: Türkçe", "Player", payload, 0);
    auto snapshot = configs::get_snapshot();
    assert(snapshot.success && snapshot.entries.size() == 1 && snapshot.entries[0].mine);
    const int id = snapshot.entries[0].id;
    configs::load(id);
    std::string loaded, name;
    assert(configs::consume_pending_payload(loaded, name) && loaded == payload && name == "Test profile");
    assert(!configs::consume_pending_payload(loaded, name));
    configs::publish(id, "Updated", "", "Player", payload, 0);
    assert(configs::get_snapshot().success && configs::get_snapshot().entries.size() == 1);
    configs::shutdown(); configs::initialize(directory);
    assert(configs::get_snapshot().entries[0].name == "Updated");
    configs::publish(id, "Invalid", "", "", "bad JSON", 0);
    assert(!configs::get_snapshot().success);
    configs::load(id);
    assert(configs::consume_pending_payload(loaded, name) && name == "Updated");
    configs::load(-1);
    assert(!configs::get_snapshot().success && !configs::consume_pending_payload(loaded, name));
    configs::remove(id);
    assert(configs::get_snapshot().success && configs::get_snapshot().entries.empty());
    configs::shutdown();
    const std::string with_binding = R"({"modules":[{"id":"click_aim_assist","enabled":true,"keybind":65,"settings":{"mode":1}}]})";
    configs::initialize(directory);
    configs::publish(0, "Private", "", "Player", with_binding, 1);
    assert(configs::get_snapshot().success);
    const auto private_id = configs::get_snapshot().entries[0].id;
    configs::load(private_id);
    assert(configs::consume_pending_payload(loaded, name));
    assert(loaded.find("\"keybind\":0") != std::string::npos && loaded.find("\"keybind\":65") == std::string::npos);
    configs::shutdown();
    const auto legacy_file = directory / (std::to_string(private_id) + ".json");
    std::string legacy;
    { std::ifstream input(legacy_file); legacy.assign(std::istreambuf_iterator<char>(input), {}); }
    const std::string zero_binding = "\\\"keybind\\\":0";
    const auto offset = legacy.find(zero_binding);
    assert(offset != std::string::npos);
    legacy.replace(offset, zero_binding.size(), "\\\"keybind\\\":65");
    { std::ofstream output(legacy_file); output << legacy; }
    configs::initialize(directory);
    assert(configs::get_snapshot().success);
    { std::ifstream input(legacy_file); legacy.assign(std::istreambuf_iterator<char>(input), {}); }
    assert(legacy.find("\\\"keybind\\\":65") == std::string::npos);
    configs::shutdown();
    const auto blocked = directory / "blocked";
    { std::ofstream stream(blocked); stream << "not a directory"; }
    configs::initialize(blocked);
    assert(!configs::get_snapshot().success);
    configs::shutdown();
}
