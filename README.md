# SearchServer
Поисковик документов с учетом минус-слов (документы с этими словами не будут отображаться в результате поисков). Работает на подобии поисковиков, такой как Яндекс. Ранжирование результатов происходит по TF-IDF.  
  
Сейчас в `main` прописан тестировщик скорости с генератором случайных значений для проверки работоспособности параллельного поиска.
## Описание
Некоторые особенности данного поисковика:
* Возвращаются топ 5 документов по релевантности - глобальная константа `MAX_RESULT_DOCUMENT_COUNT = 5`
* Учитываются стоп-слова - подобные слова не учитываются в запросе.
* Учитываются минус-слова - документы, включающие такие слова, будут исключины из результата:  
_Если в запросе нет плюс-слов, ничего найтись не должно._  
_Если одно и то же слово будет в запросе и с минусом, и без, считается, что оно есть только с минусом._
* Учёт рейтинга документа (опционально) - по функции-предикат.
* Учёт статуса документа (опционально) - по функции-предикат.
* Ранжирование по TF-IDF:  
_Полезность слов оценивают понятием inverse document frequency или IDF. Эта величина — свойство слова, а не документа. Чем в большем количестве документов есть слово, тем ниже IDF._  
_Второй способ улучшить ранжирование — выше располагать документы, где искомое слово встречается более одного раза. Здесь нужно рассчитать term frequency или TF. Для конкретного слова и конкретного документа это доля, которую данное слово занимает среди всех._  
_*При одинаковой релевантности с точностью до 10^-6, сортировка документа происходит по убыванию рейтинга._  
_Расчёт релевантности:_  
    * _вычисляется IDF каждого слова в запросе,_
    * _вычисляется TF каждого слова запроса в документе,_
    * _IDF каждого слова запроса умножается на TF этого слова в этом документе,_
    * _все произведения IDF и TF в документе суммируются._  
* Реализован механизм исключений.
* Реализован *Paginator* - поисковая система разбивает результаты на страницы.
* Разработана функция поиска и удаления дубликатов - документов, у которых наборы встречающихся слов совпадают; стоп-слова игнорируются.
> _Удаляются документы с бóльшим id._
* Реализована многопоточная версия поиска документа в дополнении к однопоточной.  
> _Исключено состояние гонки._
## Инструкция по использованию
Перед использованием измените `main` под ваши данные.
1. На вход элемента класса `SearchServer` через конструктор подаются стоп-слова;
2. Через метод `AddDocument` этого класса добавляются id, текст документа, его статус (опционально), его рейтинг (опционально);
3. Методом `FindTopDocuments` этого класса производится поиск документа по запросу;  
На вход подаются: политика параллельного выполнения(опционально), запрос, статус документа (опционально), функция предикат (опционально);
4. Вывод документа через функцию вывода.  
> Пример:
```c++
#include "process_queries.h"
#include "search_server.h"

#include <execution>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }


    cout << "ACTUAL by default:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    // параллельная версия
    for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}
```
> Вывод:  
> `ACTUAL by default:`  
> `{ document_id = 2, relevance = 0.866434, rating = 1 }`  
> `{ document_id = 4, relevance = 0.231049, rating = 1 }`  
> `{ document_id = 1, relevance = 0.173287, rating = 1 }`  
> `{ document_id = 3, relevance = 0.173287, rating = 1 }`  
> `BANNED:`  
> `Even ids:`  
> `{ document_id = 2, relevance = 0.866434, rating = 1 }`  
> `{ document_id = 4, relevance = 0.231049, rating = 1 }`
## Системные требования
- С++17 (C++1z)
## Планы по доработке
Реализовать полный ввод напрямую через командную строку, чтобы не прописывать некоторые его элементы в `main`.
***
![giffif](https://user-images.githubusercontent.com/93004994/164434944-d2e29257-6f92-4aae-a542-ecb36bd52df1.gif)
