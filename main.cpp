#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <sstream>
#include <numeric>
#include <thread>
#include <functional>

class ModelElectronics {
private:
    const int N_CHAN = 109;
    const int PULSE_LENGTH = 1000;
    const int AMP_SIZE = 10000;
    const int INTERF_LENGTH = 6200;
    const int BIN_2_GEN = 1020;
    std::vector<std::vector<float>> pieds;
    std::vector<float> curbase;
    std::vector<float> pulse;
    std::vector<float> amp;
    std::vector<int> Toff;
    std::vector<float> interf;
    std::vector<float> interf_amp;
    std::vector<int> thr;
    std::vector<float> Cal;
    int N_PHEL{};
    std::vector<int> PMTid;
    std::vector<float> T;
    float Tmin{};
    float MEAN_CURR{};
    float CURR_2_PH{};
    std::vector<std::vector<int>> data_out;
    std::vector<std::vector<float>> data;
public:
    ModelElectronics();

    void GetMoshits();

    void GetSimple(const std::string &, std::vector<float> &);

    void GetC();

    void GenerateEvent();

    std::vector<float> &getCurbaseRef() { return curbase; }

    std::vector<float> &getAmpRef() { return amp; }

    std::vector<float> &getPulseRef() { return pulse; }
};

ModelElectronics::ModelElectronics() :
        pieds(N_CHAN, std::vector<float>(2, 0)),
        curbase(N_CHAN, 0),
        pulse(PULSE_LENGTH, 0),
        amp(AMP_SIZE, 0),
        Toff(N_CHAN, 0),
        interf(INTERF_LENGTH, 0),
        interf_amp(N_CHAN, 0),
        thr(N_CHAN, 0),
        data_out(N_CHAN, std::vector<int>(BIN_2_GEN, 0)),
        data(N_CHAN, std::vector<float>(BIN_2_GEN * 25 + 2 * PULSE_LENGTH + 2, 0)) {}

void ModelElectronics::GetMoshits() {
    std::ifstream moshits("mosaic_hits_m01_Fe_1PeV_15_001_c001");

    if (!moshits.is_open()) {
        std::cerr << "Failed to open the moshits file!" << std::endl;
    }
    N_PHEL = int(std::count(std::istreambuf_iterator<char>(moshits),
                            std::istreambuf_iterator<char>(), '\n')) - 1;

    moshits.clear();
    moshits.seekg(0, std::ios::beg);

    std::string line;
    std::getline(moshits, line);
    while (std::getline(moshits, line)) {
        std::istringstream ss(line);
        double pmt_tmp, t_tmp, tmp;
        ss >> pmt_tmp;
        for (int i{0}; i < 4; i++) {
            ss >> tmp;
        }
        ss >> t_tmp;
        PMTid.push_back(int(pmt_tmp));
        T.push_back(float(t_tmp));
    }
    auto min_it = std::min_element(T.begin(), T.end());
    Tmin = *min_it;
    moshits.close();
}

void ModelElectronics::GetSimple(const std::string &fileName, std::vector<float> &vec) {
    std::ifstream input(fileName);
    if (!input.is_open()) {
        std::cerr << "Failed to open the " << fileName << " file!" << std::endl;
    }
    std::string line;
    std::getline(input, line);
    std::istringstream ss(line);
    auto it = vec.begin();
    float tmp;
    while (ss >> tmp && it != vec.end()) {
        *it = tmp;
        ++it;
    }
    input.close();
}

void ModelElectronics::GetC() {
    std::ifstream cal("14484.cal");
    if (!cal.is_open()) {
        std::cerr << "Failed to open the calibration file!" << std::endl;
    }
    std::string line;
    auto it = Cal.begin();
    while (std::getline(cal, line) && it != Cal.end()) {
        std::istringstream ss(line);
        float tmp;
        for (int i{0}; i < 13; i++) {
            ss >> tmp;
        }
        ss >> *it;
        ++it;
    }
}

void ModelElectronics::GenerateEvent() {
    for(int phid{0};phid <= N_PHEL; phid++){

    }
}

class ThreadManager {
private:
    ModelElectronics &obj;
public:
    explicit ThreadManager(ModelElectronics &objRef) : obj(objRef) {}

    void inputAll();
};

void ThreadManager::inputAll() {
    std::vector<std::thread> threads;
    threads.emplace_back(&ModelElectronics::GetC, &obj);
    threads.emplace_back(&ModelElectronics::GetMoshits, &obj);
    threads.emplace_back(&ModelElectronics::GetSimple, &obj, "CurRels.dat", std::ref(obj.getCurbaseRef()));
    threads.emplace_back(&ModelElectronics::GetSimple, &obj, "Impulse2GHz.dat", std::ref(obj.getPulseRef()));
    threads.emplace_back(&ModelElectronics::GetSimple, &obj, "AmpDistrib.dat", std::ref(obj.getAmpRef()));
    for (auto &t: threads) {
        t.join();
    }
}


int main() {
    ModelElectronics model;
    ThreadManager manager(model);
    manager.inputAll();
    return 0;
}
