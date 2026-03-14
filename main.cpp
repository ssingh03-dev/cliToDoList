#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <sqlite3.h>

#include "json.hpp"
#include "httplib.h"

// TODO migrate tasks.txt file to a SQLite database
// TODO (optional) improve performance by using modern C++20 features

// TODO create a web UI frontend that calls the JSON API
// TODO (optional) use LLM for task prioritization based on its status, description, and others

constexpr int HTTP_PORT = 8080;

// vars
using json = nlohmann::ordered_json;

// declarations
int getTaskCount();

// basically skip all the dele lines (probably just for the default/all types mode)
// also id gets potentially overwritten after each purge since it's simply a line number (technically not id)
// method when mode is r
json modeR(const int argc, char* argv[], bool returnJson = false) {
    // options going to be done, open, or iprg (in progress); and -first, -se, or -last (decided to do only one)
    // if given multiple, only the last of the two will be considered
    // all other options will be given a warning, but it continues based on the last valid one (default included)

    json tasks = json::array();  // JSON result

    std::ifstream infile("tasks.txt");

    if (argc == 2) {    // here, also add their ids next to them too

        std::cout << std::left
              << std::setw(6) << " ID"
              << std::setw(8) << "STATUS"
              << "DESCRIPTION" << std::endl;
        std::cout << "----  ------  -----------" << std::endl;

        std::string line;
        std::string status;
        int lineNum = 1;
        while (std::getline(infile, line)) {
            status = line.substr(0, 4);

            if (status != "TRSH") {
                std::cout << std::right << std::setw(4) << lineNum
                          << std::left << std::setw(10) << std::string("   ") + status
                          << line.substr(5) << std::endl;   // skipped space to better align with top

                if (returnJson) {
                    tasks.push_back(json::object({
                        {"id", lineNum},
                        {"status", status},
                        {"description", line.substr(5)},
                    }));
                }
            }

            lineNum++;
        }
    } else {
        std::string rangeMode = "default";
        std::string taskType = "default";

        int firstNumber = 1, secondNumber = getTaskCount();
        int maxNumber = secondNumber;

        std::string line;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-first") == 0) {
                if (i + 1 < argc
                    && std::all_of(argv[i + 1], argv[i + 1] + strlen(argv[i + 1]), ::isdigit)
                    && std::stoi(argv[i + 1]) <= maxNumber) {
                    rangeMode = "first";
                    firstNumber = 1;
                    secondNumber = std::stoi(argv[i + 1]);
                    i = i + 1;
                } else {
                    // Here, the first is essentially ignored
                    std::cout << "Warning: no number specified for -first (MAYBE also out of bounds)" << std::endl;
                }
            } else if (strcmp(argv[i], "-se") == 0) {
                if (i + 2 < argc
                    && std::all_of(argv[i + 1], argv[i + 1] + strlen(argv[i + 1]), ::isdigit)
                    && std::all_of(argv[i + 2], argv[i + 2] + strlen(argv[i + 2]), ::isdigit)
                    && std::stoi(argv[i + 1]) < std::stoi(argv[i + 2])
                    && std::stoi(argv[i + 2]) <= maxNumber
                    && std::stoi(argv[i + 1]) >= 0) {
                    rangeMode = "se";
                    firstNumber = std::stoi(argv[i + 1]);
                    secondNumber = std::stoi(argv[i + 2]);
                    i = i + 2;
                } else if (i + 1 < argc
                    && std::all_of(argv[i + 1], argv[i + 1] + strlen(argv[i + 1]), ::isdigit)) {
                    std::cout << "Warning: no second number specified for -se (MAYBE also out of bounds)" << std::endl;
                    i = i + 1;
                } else {
                    // Here, the se is essentially ignored
                    std::cout << "Warning: no numbers specified for -se (MAYBE also out of bounds)" << std::endl;
                }
            } else if (strcmp(argv[i], "-last") == 0) {
                if (i + 1 < argc
                    && std::all_of(argv[i + 1], argv[i + 1] + strlen(argv[i + 1]), ::isdigit)
                    && std::stoi(argv[i + 1]) >= 0) {
                    rangeMode = "last";
                    firstNumber = maxNumber - std::stoi(argv[i + 1]) + 1;
                    secondNumber = maxNumber;
                    i = i + 1;
                } else {
                    // Here, the last is essentially ignored
                    std::cout << "Warning: no number specified for -last (MAYBE also out of bounds)" << std::endl;
                }
            } else if (strcmp(argv[i], "-done") == 0) {
                taskType = "DONE";
            } else if (strcmp(argv[i], "-open") == 0) {
                taskType = "OPEN";
            } else if (strcmp(argv[i], "-iprg") == 0) {
                taskType = "IPRG";
            } else {
                std::cout << "Warning: unknown option " << argv[i] << std::endl;
            }
        }

        std::cout << "Range mode: " << rangeMode << std::endl;
        std::cout << "Task type: " << taskType << std::endl;

        for (int i = 1; i < firstNumber; i++) {
            std::getline(infile, line);
        }

        char status[5]; // 5 chars + null terminator
        if (taskType != "default") {
            for (int i = firstNumber; i <= secondNumber; i++) {
                infile.read(status, 4);
                status[4] = '\0';

                std::getline(infile, line); // probably inefficient
                if (status == taskType) {
                    std::cout << i << ":" << line << std::endl;

                    if (returnJson) {
                        tasks.push_back(json::object({
                            {"id", i},
                            {"status", status},
                            {"description", line.substr(1)},
                        }));
                    }
                }
            }
        } else {
            for (int i = firstNumber; i <= secondNumber; i++) {
                infile.read(status, 4);
                status[4] = '\0';

                std::getline(infile, line); // probably inefficient
                if (status != std::string("TRSH")) {        // so it skips soft deleted stuff here
                    std::cout << i << ":" << line << std::endl;
                    if (returnJson) {
                        tasks.push_back(json::object({
                            {"id", i},
                            {"status", status},
                            {"description", line.substr(1)},
                        }));
                    }
                }
            }
        }
    }
    infile.close();

    return tasks;
}

// method when mode is w
json modeW(const int argc, char* argv[], bool returnJson = false) {
    // the 3rd arg is the task description, and it's assumed the task description is surrounded by quotation marks
    // for extra flags, it's going to be put in a loop and only looks for one flag, open, done, or iprg
    // if nothing or invalid results only, then defaults to open flag
    std::string taskDesc = argv[2];  // ← copy immediately

    std::ofstream outfile("tasks.txt", std::ios::app);

    std::string taskType = "OPEN";  // its the default

    if (argc > 3) {     // add flags here, like changing the open to iprg or done (make into while loop like read)
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-done") == 0) {
                taskType = "DONE";
            } else if (strcmp(argv[i], "-open") == 0) {
                taskType = "OPEN";
            } else if (strcmp(argv[i], "-iprg") == 0) {
                taskType = "IPRG";
            } else {
                std::cout << "Warning: unknown option " << argv[i] << std::endl;
            }
        }
    }

    outfile << taskType << " " << taskDesc << std::endl;
    std::cout << "Task added." << std::endl;

    outfile.close();

    if (returnJson) {
        return {
            "created",
            json::object({
                {"id", getTaskCount()},
                {"status", taskType},
                {"description", taskDesc},
            })
        };
    }

    return json::object();
}

// if id hits a dead task, tell them it's been (soft)deleted, and to redo it then do the delete opt for this id again
// method when mode is u
json modeU(const int argc, char* argv[], bool returnJson = false) {
    // basically just updating the specific task by id (can get id by reading the specific task type)
    json ret = json::object();

    std::fstream file("tasks.txt", std::ios::in | std::ios::out);

    int maxNumber = getTaskCount();
    if (maxNumber >= std::stoi(argv[2]) && maxNumber > 0) {
        std::string dummy;
        for (int i = 1; i < std::stoi(argv[2]); i++) {
            std::getline(file, dummy);
        }

        auto lineStartPos = file.tellp();
        std::getline(file, dummy);

        std::string taskType = "NEXT";  // it's the default

        if (argc > 3) {
            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "-done") == 0) {
                    taskType = "DONE";
                } else if (strcmp(argv[i], "-open") == 0) {
                    taskType = "OPEN";
                } else if (strcmp(argv[i], "-iprg") == 0) {
                    taskType = "IPRG";
                } else {
                    std::cout << "Warning: unknown option " << argv[i] << std::endl;
                }
            }
        }

        file.seekp(lineStartPos);
        char status[5];
        file.read(status, 4);
        status[4] = '\0';

        if (taskType == "NEXT") {
            // Read first 4 chars
            if (std::string(status) == "OPEN") {
                taskType = "IPRG";
            } else {
                taskType = "DONE";
            }
        }

        if (std::string(status) != taskType) {
            file.seekp(lineStartPos);
            file.write(taskType.c_str(), 4);
            std::cout << "The status of task " << argv[2] << " changed from '" << status << "' to '" << taskType << "'." << std::endl;
            if (returnJson) {
                file.close();
                return {
                    json::object({
                        {"status", "changed"},
                        {"id", std::stoi(argv[2])},
                        {"old_status", status},
                        {"new_status", taskType},
                    })
                };
            }
        } else {
            std::cout << "The status of task " << argv[2] << " did not change." << std::endl;
            if (returnJson) {
                file.close();
                return {
                    json::object({
                        {"status", "unchanged"},
                        {"id", std::stoi(argv[2])},
                        {"current_status", status},
                    })
                };
            }
        }
    } else {
        std::cout << "Task number out of range." << std::endl;
    }
    file.close();

    return json::object();
}

// method to mark a line to be deleted (TRSH) or undo the DELE by going through this method again (resets to open)
json toggleDelete(const int lineNum) {    // precondition is lineNum >= 1 and lineNum <= count from count.txt file
    std::fstream file("tasks.txt", std::ios::in | std::ios::out);

    std::string dummy;
    for (int i = 1; i < lineNum; i++) {
        std::getline(file, dummy);
    }

    const auto lineStartPos = file.tellp();
    std::getline(file, dummy);

    file.seekp(lineStartPos);
    char status[5];
    file.read(status, 4);
    status[4] = '\0';

    if (std::string(status) == "TRSH") {    // maybe previous status can be preserved somehow (unique trash statuses)
        file.seekp(lineStartPos);
        file.write("OPEN", 4);
        std::cout << "Task " << lineNum << " has been undeleted." << std::endl;
        file.close();
        return json::object({
                {"action", "undeleted"},
                {"id", lineNum},
                {"status", "OPEN"},
        });
    }

    file.seekp(lineStartPos);
    file.write("TRSH", 4);
    std::cout << "Task " << lineNum << " has been deleted." << std::endl;
    file.close();
    return json::object({
            {"action", "deleted"},
            {"id", lineNum},
            {"status", "TRSH"},
    });
}

// method to specify which line(s) to soft-delete (1 or many) (basically skips it when viewing, others need changing)
json modeD(const int argc, char* argv[], bool returnJson = false) {
    json ret = json::array();

    const int maxNumber = getTaskCount();

    if (argc > 2) { // after choosing mode, it needs to specify the line numbers to be deleted
        for (int i = 2; i < argc; i++) {
            if (std::stoi(argv[i]) <= maxNumber) {
                ret.push_back(toggleDelete(std::stoi(argv[i])));
            } else {
                std::cout << "Line number out of range." << std::endl;
            }
        }
    } else {
        std::cout << "No line numbers specified." << std::endl;
    }

    if (returnJson) {
        return ret;
    }

    return json::object();
}

// method to purge the marked soft-deleted items
void purgeTrash() {
    std::ifstream infile("tasks.txt");

    std::vector<std::string> nonTrash;
    std::string line;
    while (std::getline(infile, line)) {
        if (line.substr(0, 4) != "TRSH") {
            nonTrash.push_back(line);
        }
    }
    infile.close();

    std::ofstream outfile("tasks.txt", std::ios::trunc);

    for (const auto& keptLine : nonTrash) {
        outfile << keptLine << '\n';    // more efficient than endl since buffer is not flushed
    }

    outfile.close();
}

// method to purge the marked soft-deleted items
json modeP(bool isCli = true) {
    // Prints all the marked items and confirms with the user if they still want to purge the items
    json ret = json::array();

    std::ifstream infile("tasks.txt");

    int i = 1;
    int items = 0;
    std::string line;
    while (std::getline(infile, line)) {
        if (line.substr(0, 4) == "TRSH") {
            std::cout << i << line.substr(4) << std::endl;
            items++;

            if (!isCli) {
                ret.push_back(json::object({
                    {"id", i},
                    {"description", line.substr(5)},
                }));
            }
        }
        i++;
    }

    if (items == 0) {
        std::cout << "No marked items to purge." << std::endl;
        return json::object();
    }

    if (isCli) {    // only in cli you confirm here; in http, purgeTrash is called in the delete method
        std::string confirm;
        std::cout << std::endl
                  << "Purge ALL " << items
                  << " TRASH item(s) listed above? This CANNOT be undone! (y/n): ";
        std::getline(std::cin, confirm);

        if (!confirm.empty() && (confirm[0] == 'y' || confirm[0] == 'Y')) purgeTrash();
    } else {
        std::string confirm;
        std::cout << std::endl
                  << "Purge ALL " << items
                  << " TRASH item(s) listed above? This CANNOT be undone! (If yes, paste handler after DELETE route).";
    }
    return ret;
}

// method to get number of lines (delete count.txt file)
int getTaskCount() {
    std::ifstream file("tasks.txt");
    std::string dummy;
    int count = 0;
    while (std::getline(file, dummy)) count++;
    return count;
}

// for method modes that take in CLI arguments
using modeFunc = json(*)(int, char**, bool);

// add a method to convert JSON to argv/argc for arguments; then it calls the appropriate function (modeP can be called directly)
json callMode(modeFunc mode, std::vector<std::string> cliArgs) {
    // std::ostringstream oss;
    // std::streambuf* old = std::cout.rdbuf(oss.rdbuf());  // Redirect cout

    // first add arg [0] as a placeholder
    std::vector<std::string> fullArgs;
    fullArgs.emplace_back("./todo");           // argv[0]
    fullArgs.emplace_back("a");   // argv[1] = "r" or "w"
    fullArgs.insert(fullArgs.end(),            // argv[2+] = flags
                    std::make_move_iterator(cliArgs.begin()),
                    std::make_move_iterator(cliArgs.end()));

    std::vector<char*> cargs;
    cargs.reserve(fullArgs.size());
    for (const auto& s : fullArgs) {
        cargs.push_back(const_cast<char*>(s.c_str()));
    }

    json result = mode(static_cast<int>(cargs.size()), cargs.data(), true);

    // std::cout.rdbuf(old);  // Restore

    return result;  // oss.str();
}

std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::istringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        if (!token.empty()) {
            result.push_back(token);
        }
    }
    return result;
}

std::string latestToken;

void generateToken() {
    thread_local std::mt19937 gen(std::random_device{}());
    thread_local std::uniform_int_distribution<> dis(0, 15);

    std::string token(16, '0');
    std::generate(token.begin(), token.end(),
        [&]{ return "0123456789abcdef"[dis(gen)]; });
    latestToken = token;
}

// add http method to get api calls with proper format, use the above conversion from JSON to argv, which also calls the method
void run_server() {
    // HTTP
    httplib::Server svr;

    // Get methods (still needs to be tested)
    svr.Get("/tasks", [](const httplib::Request& req, httplib::Response& res) {
        std::string firstIndex = req.get_param_value("first");
        std::string lastIndex = req.get_param_value("last");
        std::string taskType = req.get_param_value("type");

        std::vector<std::string> taskJson;

        if (!taskType.empty()) {    // type safety so incorrect stuff is not passed
            if (taskType == "open" || taskType == "OPEN") taskJson.emplace_back("-open");
            else if (taskType == "done" || taskType == "DONE") taskJson.emplace_back("-done");
            else if (taskType == "iprg" || taskType == "IPRG") taskJson.emplace_back("-iprg");
            else {
                std::cout << "Warning: unknown task type " << taskType << std::endl;
            }
        }

        // se when both provided, else use first or last
        if (!firstIndex.empty() && !lastIndex.empty()) {
            taskJson.emplace_back("-se");
            taskJson.push_back(firstIndex);
            taskJson.push_back(lastIndex);
        } else if (!firstIndex.empty()) {
            taskJson.emplace_back("-first");
            taskJson.push_back(firstIndex);
        } else if (!lastIndex.empty()) {
            taskJson.emplace_back("-last");
            taskJson.push_back(lastIndex);
        }

        res.set_content(callMode(modeR, taskJson).dump(2), "text/plain");
        res.status = 200;
    });

    // Post methods
    svr.Post("/tasks", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content("Invalid JSON\n", "text/plain");
            return;
        }

        std::string taskString = body.value("task", "");
        std::string taskType = body.value("type", "OPEN");

        std::vector<std::string> taskJson;

        taskJson.push_back(taskString);
        if (taskType == "done" || taskType == "DONE") taskJson.emplace_back("-done");
        else if (taskType == "iprg" || taskType == "IPRG") taskJson.emplace_back("-iprg");
        else taskJson.emplace_back("-open");

        res.set_content(callMode(modeW, taskJson).dump(2), "text/plain");
        res.status = 201;
    });     // works

    // Put methods
    svr.Put(R"(/tasks/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        std::string taskType = req.get_param_value("type");

        std::vector<std::string> taskJson;

        taskJson.push_back(id);
        if (!taskType.empty()) {        // anything else basically is ignored
            if (taskType == "done" || taskType == "DONE") taskJson.emplace_back("-done");
            else if (taskType == "iprg" || taskType == "IPRG") taskJson.emplace_back("-iprg");
            else if (taskType == "open" || taskType == "OPEN") taskJson.emplace_back("-open");
        }

        res.set_content(callMode(modeU, taskJson).dump(2), "text/plain");
        res.status = 200;
    });     // works

    // Delete methods (one for soft delete and one for purging the trash can (soft delete takes id, purge is nothing but needs to be confirmed in terminal))
    svr.Delete("/tasks", [](const httplib::Request& req, httplib::Response& res) {
        std::string ids = req.get_param_value("ids");
        if (ids.empty()) {
            res.status = 400;
            res.set_content("Missing ids parameter\n", "text/plain");
            return;
        }

        std::vector<std::string> taskJson = split(ids, ',');

        res.set_content(callMode(modeD, taskJson).dump(2), "text/plain");
        res.status = 200;
    });     // works

    svr.Delete("/tasks/purge", [](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_param_value("token");
        if (token.empty()) {
            // generate token here and save it in run_server variable (only latest token considered)
            generateToken();

            // std::ostringstream oss;
            // std::streambuf* old = std::cout.rdbuf(oss.rdbuf());  // Redirect cout
            json items = modeP(false);

            json ret = json::object({
                    {"status", "pending"},
                    {"trash_count", items.size()},
                    {"items", items},
                    {"confirm", {
                        {"token", latestToken},
                        {"hint", "DELETE /tasks/purge?token=" + latestToken}
                    }}
            });
            // std::cout.rdbuf(old);  // Restore

            // res.set_content(oss.str()+"\nConfirm: ?token=" + latestToken + "\n", "text/plain");
            res.set_content(ret.dump(2), "text/plain");
            return;
        }

        if (token != latestToken) {
            res.status = 401;
            res.set_content("Invalid token\n", "text/plain");
            return;
        }

        purgeTrash();

        res.set_content("OK\n", "text/plain");
        res.status = 200;
    });     // works (doesn't need to be confirmed in terminal)
    // for purge json it so for token empty it returns a json object with last one confirm, token
    // for the actual purge, return all the tasks it purged, changing status to purged or prgd

    // basic methods work, but need updating to return the information somehow
    // for purge utilize token, first request generates a token which is used in the second request as a way to confirm
    // purge token works, just need a way around the terminal needing user input afterwards

    // Run Server
    std::cout << "Starting server...\n";
    svr.listen("0.0.0.0", HTTP_PORT);  // ← BLOCKS HERE FOREVER
    std::cout << "This line never runs\n";
}

int main(const int argc, char* argv[]) {

    // sqlit
    sqlite3* db = nullptr;

    if (sqlite3_open("tasks.db", &db) != SQLITE_OK) {
        std::cout << "Error opening database." << std::endl;
        return 1;
    }

    const char* createSql = "CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY AUTOINCREMENT, task TEXT NOT NULL, type TEXT NOT NULL, date TEXT)";

    sqlite3_exec(db, createSql, nullptr, nullptr, nullptr);

    // add as parameter to methods (..., sqlite3* db) and pass it as &db (pointers are still confusing)

    // TODO also with sqlite being added, need to check logic where id is considered the same as line number

    // The first line/number of the file will be the total number of tasks. Compare with ranges to see if out of bounds.
    // Look through the note app to see what to implement.

    // The To-Do List file is system-based and automatically generated.

    if (argc > 1 && strcmp(argv[1], "--api") == 0) {
        run_server();   // blocks here until Ctrl+C
        return 0;            // never reached normally
    }

    if (argc < 2) {
        std::cout << "No mode specified." << std::endl;
         return 1;
    }

    std::ifstream infile("tasks.txt");
    if (!infile) {
        std::cout << "Task file not found. Creating one: " << std::endl;

        std::ofstream outfile("tasks.txt");
        if (!outfile) {
            std::cout << "Error creating task file." << std::endl;
            return 1;
        }

        std::cout << "Task file created, proceeding." << std::endl;
        outfile.close();
    } else {
        std::cout << "Task file found, proceeding." << std::endl;
        infile.close();
    }

    if (strcmp(argv[1], "r") == 0) {
        // check for corrections or errors, then call the method for r
        modeR(argc, argv);
    } else if (strcmp(argv[1], "w") == 0) {
        // check for corrections or errors, then call the method for w
        if (argc > 2 ) modeW(argc, argv);
        else std::cout << "No task specified." << std::endl;
    } else if (strcmp(argv[1], "u") == 0) {
        // check for corrections or errors, then call the method for u
        if (argc > 2) modeU(argc, argv);
        else std::cout << "No task/ticket number specified." << std::endl;
    } else if (strcmp(argv[1], "d") == 0) {
        modeD(argc, argv);
    } else if (strcmp(argv[1], "p") == 0) {
        modeP();
    } else {
        std::cout << "Invalid mode specified." << std::endl;
        return 1;
    }

    return 0;
}