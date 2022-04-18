#pragma once
 
#include <map>
#include <algorithm>
#include <execution>
 
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
 
class SearchServer {
public:
 
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
 
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
 
    template <typename DocumentPredicate>
std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;
 
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentStatus status) const;
    
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query) const;
 
    int GetDocumentCount() const;
 
    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;
    
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    
    void RemoveDocument(int document_id);

    template<class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
 
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    template<class ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy&& policy, const std::string_view raw_query, int document_id) const;
 
private:
 
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_; //да, он хранит исходный текст
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
 
    bool IsStopWord(const std::string_view word) const;
 
    static bool IsValidWord(const std::string_view word);
 
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;
 
    static int ComputeAverageRating(const std::vector<int>& ratings);
 
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
 
    QueryWord ParseQueryWord(const std::string_view text) const;
 
    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };
 
    Query ParseQuery(const std::string_view text) const;
 
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;
 
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
};
 
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}
 
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
 
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
 
    sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
 
    return matched_documents;
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
    }

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return SearchServer::FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;

    for_each (query.plus_words.begin(), query.plus_words.end(), 
    [this, &document_predicate, &document_to_relevance] (const std::string_view& word) {
        if (word_to_document_freqs_.count(std::string(word)) != 0) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
    });

    for_each (query.minus_words.begin(), query.minus_words.end(),
    [this, &document_to_relevance] (const std::string_view& word) {
        if (word_to_document_freqs_.count(std::string(word)) != 0) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
                document_to_relevance.erase(document_id);
            }
        }
    });
    
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
    static constexpr int MINUS_LOCK_COUNT = 16;
    ConcurrentMap<int, int> minus_ids(MINUS_LOCK_COUNT);
    for_each(
        std::execution::par,
        query.minus_words.begin(),
        query.minus_words.end(),
        [this, &minus_ids](const std::string_view word) {
            if (word_to_document_freqs_.count(std::string(word))) {
                for (const auto& document_freqs : word_to_document_freqs_.at(std::string(word))) {
                    minus_ids[document_freqs.first];
                }
            }
        }
    );

    auto minus = minus_ids.BuildOrdinaryMap();
    
    static constexpr int PLUS_LOCK_COUNT = 10000;
    ConcurrentMap<int, double> document_to_relevance(PLUS_LOCK_COUNT);
    static constexpr int PART_COUNT = 16;
    const auto part_length = query.plus_words.size() / PART_COUNT;
    auto part_begin = query.plus_words.begin();
    auto part_end = next(part_begin, part_length);
    
    std::vector<std::future<void>> futures;
    for (int i = 0; 
        i < PART_COUNT; 
        ++i, part_begin = part_end, part_end = (i == PART_COUNT - 1 ? query.plus_words.end() : next(part_begin, part_length))
        ) {
        futures.push_back(std::async([this, part_begin, part_end, &document_predicate, &document_to_relevance, &minus] {
            for_each(std::execution::par,
                    part_begin, 
                    part_end, 
                    [this, &document_predicate, &document_to_relevance, &minus] (std::string_view word) {
                    if (word_to_document_freqs_.count(std::string(word))) {
                        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
                            const auto& document_data = documents_.at(document_id);
                            if (document_predicate(document_id, document_data.status, document_data.rating) && (minus.count(document_id) == 0)) {
                                document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                            }
                        }
                    }
            });
        }));
    }
    
    for (auto& stage : futures) {
        stage.get();
    }
    
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id){
	auto it = document_ids_.find(document_id);
	if (it == document_ids_.end()) {
		return;
	}
	
	auto& items = document_to_word_freqs_.at(document_id);
	
	std::vector<const std::string*> words(items.size());
	std::transform(policy, items.begin(), items.end(), words.begin(), [](auto& p) { return &p.first; });
	
	std::for_each(policy, words.begin(), words.end(),
		[&](auto word) {
			word_to_document_freqs_.at(*word).erase(document_id);
		}
	);

	document_ids_.erase(document_id);
	documents_.erase(document_id);
	document_to_word_freqs_.erase(document_id);
}