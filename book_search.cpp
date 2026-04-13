#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <omp.h>
#include <nlohmann/json.hpp>
#include <thread>  // Добавьте эту строку

using json = nlohmann::json;
using namespace std;

struct Book {
    int id;
    string title;
    string author;
    string genre;
    int year;
    int pages;
    double rating;
    string publisher;
};

class BookSearcher {
private:
    vector<Book> books;

public:
    bool loadBooksFromJSON(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cout << "Ошибка: Не удалось открыть файл " << filename << endl;
            return false;
        }

        try {
            json j;
            file >> j;
            
            for (const auto& item : j) {
                Book book;
                book.id = item["id"];
                book.title = item["title"];
                book.author = item["author"];
                book.genre = item["genre"];
                book.year = item["year"];
                book.pages = item["pages"];
                book.rating = item["rating"];
                book.publisher = item["publisher"];
                books.push_back(book);
            }
            
            cout << "Загружено книг: " << books.size() << endl;
            return true;
        }
        catch (const exception& e) {
            cout << "Ошибка при парсинге JSON: " << e.what() << endl;
            return false;
        }
    }

    vector<Book> searchSequential(const string& author, const string& genre, 
                                 double minRating, int minYear, int maxYear, 
                                 int minPages, int maxPages) {
        vector<Book> result;
        
        for (const auto& book : books) {
            bool matches = true;
            
            if (!author.empty() && book.author.find(author) == string::npos) {
                matches = false;
            }
            if (!genre.empty() && book.genre != genre) {
                matches = false;
            }
            if (book.year < minYear || book.year > maxYear) {
                matches = false;
            }
            if (book.rating < minRating) {
                matches = false;
            }
            if (book.pages < minPages || book.pages > maxPages) {
                matches = false;
            }
            
            if (matches) {
                result.push_back(book);
            }
        }
        
        return result;
    }

    vector<Book> searchParallel(const string& author, const string& genre,
                               double minRating, int minYear, int maxYear,
                               int minPages, int maxPages) {
        vector<Book> result;
        
        #pragma omp parallel
        {
            vector<Book> localResult;
            
            #pragma omp for nowait
            for (size_t i = 0; i < books.size(); i++) {
                const auto& book = books[i];
                bool matches = true;
                
                if (!author.empty() && book.author.find(author) == string::npos) {
                    matches = false;
                }
                if (!genre.empty() && book.genre != genre) {
                    matches = false;
                }
                if (book.year < minYear || book.year > maxYear) {
                    matches = false;
                }
                if (book.rating < minRating) {
                    matches = false;
                }
                if (book.pages < minPages || book.pages > maxPages) {
                    matches = false;
                }
                
                if (matches) {
                    localResult.push_back(book);
                }
            }
            
            #pragma omp critical
            {
                result.insert(result.end(), localResult.begin(), localResult.end());
            }
        }
        
        return result;
    }

    void showResults(const vector<Book>& results, const string& searchType, 
                    chrono::duration<double> elapsed) {
        cout << "\n=== " << searchType << " ===" << endl;
        
        // Выводим время в микросекундах
        auto microsec = chrono::duration_cast<chrono::microseconds>(elapsed);
        auto millisec = chrono::duration_cast<chrono::milliseconds>(elapsed);
        
        if (millisec.count() > 0) {
            cout << "Время: " << millisec.count() << " мс";
            if (microsec.count() % 1000 != 0) {
                cout << " (" << microsec.count() << " мкс)";
            }
        } else {
            cout << "Время: " << microsec.count() << " мкс";
        }
        cout << endl;
        
        cout << "Найдено книг: " << results.size() << endl;
        
        if (results.size() > 0) {
            cout << "\nРезультаты:" << endl;
            for (size_t i = 0; i < min(results.size(), size_t(5)); i++) {
                const auto& book = results[i];
                cout << "   " << (i+1) << ". " << book.title << " - " << book.author 
                     << " (" << book.year << ") - " << book.rating << "/10" << endl;
            }
            if (results.size() > 5) {
                cout << "   ... и еще " << (results.size() - 5) << " книг" << endl;
            }
        }
        cout << "──────────────────────────────" << endl;
    }
    
    // Функция для прогрева OpenMP
    void warmupOpenMP() {
        cout << "Прогрев OpenMP..." << endl;
        
        // Простая параллельная операция для инициализации потоков
        vector<int> dummy(1000);
        #pragma omp parallel for
        for (size_t i = 0; i < dummy.size(); ++i) {
            dummy[i] = i * 2;
        }
        
        cout << "Прогрев завершен" << endl;
    }
};

int main() {
    cout << "Параллельный поиск книг" << endl;
    cout << "============================" << endl;
    
    BookSearcher searcher;
    
    if (!searcher.loadBooksFromJSON("books.json")) {
        return 1;
    }

    // ПРОГРЕВ OpenMP перед основными измерениями
    searcher.warmupOpenMP();
    
    // Даем системе "остыть" после прогрева
    this_thread::sleep_for(std::chrono::milliseconds(100)); //ругается

    string author = "Brandon Sanderson";
    string genre = ""; 
    double minRating = 8.0;
    int minYear = 2000;
    int maxYear = 2025;
    int minPages = 0;
    int maxPages = 1000;

    cout << "\nПараметры поиска:" << endl;
    cout << "   Автор: " << (author.empty() ? "любой" : author) << endl;
    cout << "   Жанр: " << (genre.empty() ? "любой" : genre) << endl;
    cout << "   Годы: " << minYear << "-" << maxYear << endl;
    cout << "   Страницы: " << minPages << "-" << maxPages << endl;
    cout << "   Мин. рейтинг: " << minRating << endl;
    cout << "   Число потоков: " << omp_get_max_threads() << endl;

    // Измеряем время в main, как в оригинале
    auto start = chrono::high_resolution_clock::now();
    auto seqResults = searcher.searchSequential(author, genre, minRating, minYear, maxYear, minPages, maxPages);
    auto seqTime = chrono::high_resolution_clock::now() - start;
    
    start = chrono::high_resolution_clock::now();
    auto parResults = searcher.searchParallel(author, genre, minRating, minYear, maxYear, minPages, maxPages);
    auto parTime = chrono::high_resolution_clock::now() - start;

    searcher.showResults(seqResults, "ПОСЛЕДОВАТЕЛЬНЫЙ ПОИСК", seqTime);
    searcher.showResults(parResults, "ПАРАЛЛЕЛЬНЫЙ ПОИСК", parTime);

    if (seqResults.size() == parResults.size()) {
        double seqSeconds = chrono::duration<double>(seqTime).count();
        double parSeconds = chrono::duration<double>(parTime).count();
        
        if (parSeconds > 0) {
            double speedup = seqSeconds / parSeconds;
            cout << "Ускорение: " << speedup << "x" << endl;
            
            if (speedup < 0.5) {
                cout << "ВНИМАНИЕ: Параллельная версия работает медленнее!" << endl;
                cout << "Причина: 1000 книг слишком мало для эффективного распараллеливания." << endl;
                cout << "Накладные расходы OpenMP превышают выгоду от параллелизма." << endl;
            } else if (speedup > 1.5) {
                cout << "Отлично! Параллельная версия работает быстрее." << endl;
            }
        } else {
            cout << "Время параллельного поиска слишком мало для расчета ускорения" << endl;
        }
        cout << "Результаты идентичны!" << endl;
    } else {
        cout << "Результаты различаются!" << endl;
        cout << "Последовательный: " << seqResults.size() << " книг" << endl;
        cout << "Параллельный: " << parResults.size() << " книг" << endl;
    }

    // Дополнительный тест: поиск по жанру
    cout << "\n\nДополнительный поиск: книги жанра Fantasy" << endl;
    cout << "=========================================" << endl;
    
    start = chrono::high_resolution_clock::now();
    auto fantasySeq = searcher.searchSequential("", "Fantasy", 0.0, 1900, 2025, 0, 5000);
    auto fantasySeqTime = chrono::high_resolution_clock::now() - start;
    
    start = chrono::high_resolution_clock::now();
    auto fantasyPar = searcher.searchParallel("", "Fantasy", 0.0, 1900, 2025, 0, 5000);
    auto fantasyParTime = chrono::high_resolution_clock::now() - start;
    
    searcher.showResults(fantasySeq, "ПОСЛЕДОВАТЕЛЬНЫЙ (Fantasy)", fantasySeqTime);
    searcher.showResults(fantasyPar, "ПАРАЛЛЕЛЬНЫЙ (Fantasy)", fantasyParTime);
    
    if (fantasySeq.size() == fantasyPar.size()) {
        double seqSec = chrono::duration<double>(fantasySeqTime).count();
        double parSec = chrono::duration<double>(fantasyParTime).count();
        
        if (parSec > 0) {
            double speedup = seqSec / parSec;
            cout << "Ускорение для поиска по жанру: " << speedup << "x" << endl;
        }
    }

    return 0;
}