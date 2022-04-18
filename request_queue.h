#pragma once
 
#include <string>
#include <vector>
#include <deque>
 
#include "search_server.h"
 
class RequestQueue {
public:
 
    explicit RequestQueue(const SearchServer& search_server);
 
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
 
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
 
    std::vector<Document> AddFindRequest(const std::string& raw_query);
 
    int GetNoResultRequests() const;
 
private:
 
    std::deque<bool> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& search_server_;
    int seconds = 0;
}; 
 
template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
 
    std::vector<Document> docs = search_server_.FindTopDocuments(raw_query, document_predicate);
 
    seconds++;
 
    while (seconds > sec_in_day_) {
        requests_.pop_front();
        seconds--;
    }
 
    docs.empty() ? requests_.push_back(0) : requests_.push_back(1);
 
    return docs;
 
}