#include "process_queries.h"

#include <execution>
#include <iterator>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries){
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, 
                   std::make_move_iterator(queries.begin()), std::make_move_iterator(queries.end()),
                   std::make_move_iterator(result.begin()), 
                   [&search_server](const std::string &query) { 
                       return search_server.FindTopDocuments(query);}
                  );   
    return result;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    auto process_queries = ProcessQueries(search_server, queries);
    std::vector<Document> result;
    for (auto vector_result: process_queries) {
        for (auto document: vector_result) {
            result.push_back(document);
        } 
    }
    return result;
}