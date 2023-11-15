#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void PrintUnknownIncludeFileError(const string& unknown_file_name,
                                  const string& calling_file_path, int line_number) {
    cout << "unknown include file "s << unknown_file_name
        << " at file "s << calling_file_path
        << " at line "s << line_number << endl;
}

bool FindIncludeFile(const path& in_file, ifstream& base_in, ofstream& out, const vector<path>& include_directories) {

    static regex include_type1(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex include_type2(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    string reading_line;
    int line_counter = 0;

    while (getline(base_in, reading_line)) {
        ++line_counter;
        smatch m1, m2;
        if (!regex_match(reading_line, m1, include_type1) && !regex_match(reading_line, m2, include_type2)) {
            out << reading_line << endl;
        } else {
            path including_file_name, including_file_path;
            ifstream in;

            if (!m1.empty()) {
                // ищем файлы в этой директории
                // если не нашли, то вызываем базовый поиск в инклуд директориях
                including_file_name = string(m1[1]);
                including_file_path = in_file.parent_path() / including_file_name;
                if (exists(including_file_path)) {
                    in.open(including_file_path);
                }
            } else {
                including_file_name = string(m2[1]);
            }

            for (const auto& dir : include_directories) {
                if (in.is_open() && in) {
                    break;
                }
                including_file_path = dir / including_file_name;
                if (exists(including_file_path)) {
                    in.open(including_file_path);
                }
            }

            if (!in.is_open() || !in) {
                PrintUnknownIncludeFileError(including_file_name.string(), in_file.string(), line_counter);
                return false;
            } else {
                if (!FindIncludeFile(including_file_path, in, out, include_directories)) {
                    return false;
                }
            }
        }
    }
    return true;
}

// напишите эту функцию
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in(in_file);
    if (!in) {
        return false;
    }

    ofstream out(out_file);
    if (!out) {
        return false;
    }

    return FindIncludeFile(in_file, in, out, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    cout << GetFileContents("sources/a.in"s) << endl;
    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;
    cout << GetFileContents("sources/a.in"s) << endl;
    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
