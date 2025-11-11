//const char* url = "https://speed.cloudflare.com/__down?bytes=10000000"; // 10 MB
#include <cstddef>
#include <iostream>
#include <curl/curl.h>
#include <atomic>
#include <chrono>

static std::atomic<unsigned long long> TOTAL_BYTES{0}; // 64 bits

static size_t write_sink(char* ptr , size_t size, size_t nmemb, void* userdata ){
    const size_t n = size*nmemb;
    TOTAL_BYTES.fetch_add(n , std::memory_order_relaxed);
    return n;
}

int main(){
    const char* url = "https://speed.cloudflare.com/__down?bytes=10000000";
    curl_global_init(CURL_GLOBAL_ALL);

    CURL *h = curl_easy_init();
    if(!h){
        std::cerr << "curl_easy_init() Failed\n";
        curl_global_cleanup();
        return -1;
    }

    curl_easy_setopt(h, CURLOPT_URL, url);

    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "identity");

    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION , &write_sink);

    //stopclocl
    auto ts = std::chrono::steady_clock::now();

    CURLcode rc = curl_easy_perform(h);

    //stopping stopclocl
    auto tsp = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(tsp-ts).count();

    unsigned long long bytes = TOTAL_BYTES.load();
    double mbps = (bytes*8)/seconds/1e6;

    if(rc != CURLE_OK){
        std::cerr << "curl error " << curl_easy_strerror(rc) << "\n" ;
    }else{
        std::cout << "Total downloaded : " << TOTAL_BYTES.load() << " Bytes\n";
        std::cout << "Time(s) : " << seconds << " seconds\n";
        std::cout << "Mbps : " << mbps << " Mbps\n";
    }

    curl_easy_cleanup(h);

    curl_global_cleanup();

    return rc == CURLE_OK ? 0:1;

}
