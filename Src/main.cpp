//const char* url = "https://speed.cloudflare.com/__down?bytes=10000000"; // 10 MB
#include <cstddef>
#include <iostream>
#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <iomanip>

static std::atomic<unsigned long long> TOTAL_BYTES{0}; // 64 bits

static size_t write_sink(char* ptr , size_t size, size_t nmemb, void* userdata ){
    const size_t n = size*nmemb;
    TOTAL_BYTES.fetch_add(n , std::memory_order_relaxed);
    return n;
}

//dltotal, dlnow, ultotal, ulnow
// void show_progress_test(curl_off_t dltotal , curl_off_t dlnow, curl_off_t ultotal , curl_off_t ulnow){
//     std::cout << "Total downloaded " << dltotal << " \n";
//     std::cout << "Current downloaded " << dlnow << " \n";
// }

int progress_callback (void* clientp,curl_off_t dltotal , curl_off_t dlnow, curl_off_t ultotal , curl_off_t ulnow ){
    using clock = std::chrono::steady_clock;
    const clock::time_point* start_tp = static_cast<const clock::time_point*>(clientp);

    double elapsed = std::chrono::duration<double>(clock::now() - *start_tp).count();
    if (elapsed <= 0.0) elapsed = 1e-9;

    //printing
    static clock::time_point last_print{};
    const auto now = clock::now();
    if(last_print.time_since_epoch().count() != 0){
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_print).count();
        if(ms < 200) return 0;
    }
    last_print = now;

    const double mbps = (static_cast<double>(dlnow) * 8.0) / (elapsed * 1e6);
    const double mb_so_far = static_cast<double>(dlnow) / 1e6;
    const bool   have_total = (dltotal > 0);
    const double mb_total = have_total ? static_cast<double>(dltotal) / 1e6 : 0.0;
    const double pct = have_total ? (100.0 * static_cast<double>(dlnow) / static_cast<double>(dltotal)) : 0.0;

    std::cout << '\r' << std::fixed << std::setprecision(2);
    if (have_total) {
        std::cout << "[ " << pct << "% ] " << mbps << " Mbps ("
                    << mb_so_far << " MB / " << mb_total << " MB)";
    } else {
        std::cout << mbps << " Mbps (" << mb_so_far << " MB)";
    }
    std::cout.flush(); // make sure it shows immediately

    return 0; // returning 0 tells libcurl: "keep going"
}

int main(){
    const char* url = "https://speed.cloudflare.com/__down?bytes=100000000";
    curl_global_init(CURL_GLOBAL_ALL);

    CURL *h = curl_easy_init();
    if(!h){
        std::cerr << "curl_easy_init() Failed\n";
        curl_global_cleanup();
        return -1;
    }

    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "identity");

    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION , &write_sink);

    //stopclocl
    auto ts = std::chrono::steady_clock::now();
    curl_easy_setopt(h, CURLOPT_XFERINFODATA , &ts);

    CURLcode rc = curl_easy_perform(h);
    std::cout << '\n';

    //stopping stopclocl
    auto tsp = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(tsp-ts).count();

    unsigned long long bytes = TOTAL_BYTES.load();
    double mbps = (static_cast<double>(bytes)*8.0)/seconds/1e6;

    if(rc != CURLE_OK){
        std::cerr << "curl error " << curl_easy_strerror(rc) << "\n" ;
    }else{
        std::cout << "Total downloaded : " << TOTAL_BYTES.load() << " Bytes\n";
        std::cout << "Time(s) : " << seconds << " seconds\n";
        std::cout << "Mbps : " << mbps << " Mbps\n";
    }

    curl_easy_cleanup(h);

    curl_global_cleanup();
    // show_progress_test(10000000, 3000000, 0, 0);
    return rc == CURLE_OK ? 0:1;

}
