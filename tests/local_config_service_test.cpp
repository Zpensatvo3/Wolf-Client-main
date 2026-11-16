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
    const auto blocked = directory / "blocked";
    { std::ofstream stream(blocked); stream << "not a directory"; }
    configs::initialize(blocked);
    assert(!configs::get_snapshot().success);
    configs::shutdown();
}
