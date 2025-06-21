#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>

using namespace std;

struct Table {
    string name;
    vector<string> columns;
};

map<string, Table> catalog;

string ltrim(const string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    return (start == string::npos) ? "" : s.substr(start);
}

string toUpper(string str) {
    transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}
  
void saveTable(const Table& table) {
    ofstream schema("catalog.txt", ios::app);
    schema << table.name;
    for (const string& col : table.columns) {
        schema << "," << col;
    }
    schema << endl;
}

void createTable(const string& command) {
    stringstream ss(command);
    string token, tableName;
    ss >> token >> token; // Skip "CREATE TABLE"
    ss >> tableName;

    string columnsDef;
    getline(ss, columnsDef, '(');
    getline(ss, columnsDef, ')');

    stringstream colStream(columnsDef);
    vector<string> columns;
    while (getline(colStream, token, ',')) {
        token.erase(remove(token.begin(), token.end(), ' '), token.end());
        columns.push_back(token);
    }

    Table table{tableName, columns};
    catalog[tableName] = table;

    saveTable(table);

    ofstream tableFile(tableName + ".txt");
    tableFile.close();

    cout << "Table " << tableName << " created.\n";
}

void insertInto(const string& command) {
    string upperCommand = toUpper(command);

    size_t posTable = upperCommand.find("INTO");
    size_t posValues = upperCommand.find("VALUES");

    if (posTable == string::npos || posValues == string::npos) {
        cout << "Invalid INSERT syntax.\n";
        return;
    }

    string tableName = command.substr(posTable + 4, posValues - (posTable + 4));
    tableName.erase(remove(tableName.begin(), tableName.end(), ' '), tableName.end());

    size_t start = command.find('(', posValues);
    size_t end = command.find(')', start);
    if (start == string::npos || end == string::npos || end <= start + 1) {
        cout << "Invalid VALUES syntax.\n";
        return;
    }

    string valuesStr = command.substr(start + 1, end - start - 1);
    stringstream valStream(valuesStr);
    vector<string> values;
    string token;

    while (getline(valStream, token, ',')) {
        token.erase(remove(token.begin(), token.end(), ' '), token.end());
        values.push_back(token);
    }

    ofstream outFile(tableName + ".txt", ios::app);
    if (!outFile.is_open()) {
        cout << "Could not open file for table " << tableName << endl;
        return;
    }

    for (size_t i = 0; i < values.size(); ++i) {
        outFile << values[i];
        if (i < values.size() - 1) outFile << ",";
    }
    outFile << endl;
    outFile.close();

    cout << "Inserted into " << tableName << ".\n";
}

void selectFrom(const string& command) {
    string upper = toUpper(command);
    size_t posFrom = upper.find("FROM");
    if (posFrom == string::npos) {
        cout << "Invalid SELECT syntax.\n";
        return;
    }

    string tableName = command.substr(posFrom + 4);
    tableName = ltrim(tableName);

    ifstream inFile(tableName + ".txt");
    if (!inFile) {
        cout << "Table " << tableName << " does not exist.\n";
        return;
    }

    string line;
    bool found = false;
    cout << "Data from table " << tableName << ":\n";
    while (getline(inFile, line)) {
        found = true;
        cout << line << endl;
    }

    if (!found) {
        cout << "(no data found)\n";
    }
}

// Assume the Table struct and catalog map are defined elsewhere as before

void deleteFrom(const string& command) {
    stringstream ss(command);
    string token, tableName;

    ss >> token >> token; // Consume "DELETE" and "FROM"

    if (!(ss >> tableName)) {
        cout << "Error: Table name missing in DELETE FROM command.\n";
        return;
    }

    // Extract WHERE clause (case-insensitive)
    string whereClause;
    size_t wherePos = command.find("WHERE");
    if (wherePos == string::npos) {
        wherePos = command.find("where");
    }

    if (wherePos != string::npos) {
        whereClause = command.substr(wherePos + 6);
    }

    ifstream inFile(tableName + ".txt");
    if (!inFile) {
        cout << "Error: Table " << tableName << " does not exist.\n";
        return;
    }

    ofstream tempFile("temp_" + tableName + ".txt");
    if (!tempFile) {
        cout << "Error: Could not create temporary file.\n";
        inFile.close();
        return;
    }

    vector<string> columns;
    if (catalog.count(tableName)) {
        columns = catalog[tableName].columns;
    }

    string line;
    bool deleted = false;
    bool whereColumnValid = true;

    while (getline(inFile, line)) {
        if (whereClause.empty()) {
            // No WHERE clause, delete all
            deleted = true;
            continue;
        } else {
            size_t equalsPos = whereClause.find('=');
            if (equalsPos != string::npos) {
                string whereColumnName = ltrim(whereClause.substr(0, equalsPos));
                string whereValue = ltrim(whereClause.substr(equalsPos + 1));

                // Strip spaces
                whereColumnName.erase(remove_if(whereColumnName.begin(), whereColumnName.end(), ::isspace), whereColumnName.end());
                whereValue.erase(remove_if(whereValue.begin(), whereValue.end(), ::isspace), whereValue.end());

                auto it = find(columns.begin(), columns.end(), whereColumnName);
                if (it != columns.end()) {
                    size_t columnIndex = distance(columns.begin(), it);
                    stringstream lineStream(line);
                    string value;
                    vector<string> rowValues;
                    while (getline(lineStream, value, ',')) {
                        rowValues.push_back(value);
                    }

                    if (columnIndex < rowValues.size() && rowValues[columnIndex] == whereValue) {
                        deleted = true; // Row matches, so skip writing it (i.e., delete it)
                    } else {
                        tempFile << line << endl; // Keep non-matching row
                    }
                } else {
                    whereColumnValid = false;
                    tempFile << line << endl; // Keep the row for safety
                }
            } else {
                cout << "Error: Invalid WHERE clause format.\n";
                tempFile << line << endl;
            }
        }
    }

    inFile.close();
    tempFile.close();

    // Replace only if column was valid
    if (!whereColumnValid) {
        cout << "Error: Column in WHERE clause does not exist in table '" << tableName << "'.\n";
        remove(("temp_" + tableName + ".txt").c_str()); // Clean up
        return;
    }

    // Replace original file
    if (remove((tableName + ".txt").c_str()) == 0) {
        if (rename(("temp_" + tableName + ".txt").c_str(), (tableName + ".txt").c_str()) != 0) {
            cerr << "Error renaming temporary file.\n";
        } else {
            if (deleted) {
                cout << "Deleted from " << tableName;
                if (!whereClause.empty()) {
                    cout << " where " << whereClause;
                }
                cout << ".\n";
            } else {
                cout << "No matching rows found in table " << tableName << ".\n";
            }
        }
    } else {
        cerr << "Error deleting original table file.\n";
    }
}


void printHelp() {
    cout << "\nMiniSQL Commands:\n"
         << "  CREATE TABLE table_name (col1,col2,...)\n"
         << "  INSERT INTO table_name VALUES (val1,val2,...)\n"
         << "  SELECT * FROM table_name\n"
         << "  DELETE FROM table_name\n"
         << "  EXIT\n\n";
}

int main() {
    // Load existing tables at startup
    ifstream catalogFile("catalog.txt");
    if (catalogFile) {
        string line;
        while (getline(catalogFile, line)) {
            stringstream ss(line);
            Table table;
            getline(ss, table.name, ',');
            string col;
            while (getline(ss, col, ',')) {
                table.columns.push_back(col);
            }
            catalog[table.name] = table;
        }
    }

    cout << "Welcome to MiniSQL Engine\n";
    printHelp();

    string input;
    while (true) {
        cout << "MiniSQL> ";
        getline(cin, input);

        string trimmed = ltrim(input);
        string upperInput = toUpper(trimmed);

        if (upperInput.rfind("CREATE TABLE", 0) == 0) {
            createTable(input);
        } else if (upperInput.rfind("INSERT INTO", 0) == 0) {
            insertInto(input);
        } else if (upperInput.rfind("SELECT * FROM", 0) == 0) {
            selectFrom(input);
        } else if (upperInput.find("DELETE FROM") == 0) {
            deleteFrom(trimmed); // Pass the original trimmed command
        } else if (upperInput == "EXIT") {
            break;
        } else {
            cout << "Invalid command.\n";
            printHelp();
        }
    }

    return 0;
}
