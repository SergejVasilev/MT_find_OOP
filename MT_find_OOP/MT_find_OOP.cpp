#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <regex>
#include <atomic>
#include <algorithm>

/* Написать программу mtfind, производящую поиск подстроки в текстовом файле по маске с использованием многопоточности.
 Маска - это строка, где "?" обозначает любой символ.
 Программа принимает в качестве параметров командной строки:
 1. Имя текстового файла, в котором должен идти поиск (размер файла - до 1Гб).
 2. Маску для поиска, в кавычках. Максимальная длина маски 1000 символов.
 Вывод программы должен быть в следующем формате:
 На первой строке - количество найденных вхождений.
 Далее информация о каждом вхождении, каждое на отдельной строке, через пробел: номер строки, позиция в строке, само
 найденное вхождение.
 Порядок вывода найденных вхождений должен совпадать с их порядком в файле
 Вся нумерация ведется начиная с 1 (делаем программу для обычных людей)
 Дополнения:
 В текстовом файле кодировка только 7-bit ASCII
 Поиск с учетом регистра
 Каждое вхождение может быть только на одной строке. Маска не может содержать символа перевода строки
 Найденные вхождения не должны пересекаться. Если в файле есть пересекающиеся вхождения то нужно вывести одно из них
 (любое).
 Пробелы и разделители участвуют в поиске наравне с другими символами.
 Можно использовать STL, Boost, возможности С++1x, C++2x.
 Многопоточность нужно использовать обязательно. Однопоточные решения засчитываться не будут.
 Серьезным плюсом будет разделение работы между потоками равномерно вне зависимости от количества строк во входном
 файле*/


// Структура для хранения результата
struct MatchResult {
    size_t line_number = 0;
    size_t position = 0;
    std::string match = "";
};

// Класс для управления многопоточным поиском
class MultiThreadedFinder {
public:
    MultiThreadedFinder(const std::string& filename, const std::string& mask)
        : filename_(filename), mask_(mask), match_count_(0) {
        prepare_pattern();
        read_file();
    }

    void search() {
        size_t num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) {
            num_threads = 1; // Устанавливаем значение по умолчанию, если количество потоков невозможно определить
        }
        size_t lines_per_thread = (lines_.size() + num_threads - 1) / num_threads;

        std::vector<std::thread> threads;
        for (size_t i = 0; i < num_threads; ++i) {
            size_t start_line = i * lines_per_thread;
            size_t end_line = std::min(start_line + lines_per_thread, lines_.size());
            if (start_line < lines_.size()) {
                threads.emplace_back(&MultiThreadedFinder::search_in_lines, this, start_line, end_line);
            }
        }

        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        std::sort(results_.begin(), results_.end(), [](const MatchResult& a, const MatchResult& b) {
            return a.line_number < b.line_number;
            });

        print_results();
    }

private:
    void prepare_pattern() {
        std::string regex_pattern;
        for (char ch : mask_) {
            if (ch == '?') {
                regex_pattern += '.'; // Любой символ
            }
            else {
                regex_pattern += std::regex_replace(std::string(1, ch), std::regex(R"([\\^$.|?*+()\[\]{}])"), R"(\\$&)");
            }
        }
        pattern_ = std::regex(regex_pattern);
    }

    void read_file() {
        std::ifstream file(filename_);
        if (!file) {
            throw std::runtime_error("Cannot open file " + filename_);
        }

        std::string line;
        while (std::getline(file, line)) {
            lines_.push_back(line);
        }
    }

    void search_in_lines(size_t start_line, size_t end_line) {
        std::vector<MatchResult> local_results;
        for (size_t i = start_line; i < end_line; ++i) {
            auto it = std::sregex_iterator(lines_[i].begin(), lines_[i].end(), pattern_);
            auto end = std::sregex_iterator();
            for (; it != end; ++it) {
                local_results.push_back({ i + 1, static_cast<size_t>(it->position()) + 1, it->str() });
                ++match_count_;
            }
        }

        {
            std::lock_guard<std::mutex> lock(result_mutex_);
            results_.insert(results_.end(), local_results.begin(), local_results.end());
        }
    }

    void print_results() const {
        std::cout << match_count_ << "\n";
        for (const auto& result : results_) {
            std::cout << result.line_number << " " << result.position << " " << result.match << "\n";
        }
    }

    std::string filename_;
    std::string mask_;
    std::regex pattern_;
    std::vector<std::string> lines_;
    std::vector<MatchResult> results_;
    std::atomic<size_t> match_count_;
    std::mutex result_mutex_;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: mtfind <filename> <mask>\n";
        return 1;
    }

    try {
        MultiThreadedFinder finder(argv[1], argv[2]);
        finder.search();
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
