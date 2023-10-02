#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <sstream>
#include <numeric>
#include <thread>
#include <functional>
#include <random>

std::vector<std::vector<int>> transpose(const std::vector<std::vector<int>>& matrix){
    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    auto numRows = matrix.size();
    auto numCols = matrix[0].size();

    std::vector<std::vector<int>> transposedMatrix(numCols, std::vector<int>(numRows));

    for (int i = 0; i < numRows; ++i) {
        for (int j = 0; j < numCols; ++j) {
            transposedMatrix[j][i] = matrix[i][j];
        }
    }

    return transposedMatrix;
}

/**
 * @brief Класс для моделирования электроники
 *
 * Данный класс используется для загрузки входных параметров
 * и расчета конечных данных.
 *
 */
class ModelElectronics {
private:
    /* Константы для расчетов. */
    const int N_CHAN = 109; /// Число каналов
    const int PULSE_LENGTH = 1000;
    const int AMP_SIZE = 10000;
    const int INTERF_LENGTH = 6200;
    const int BIN_2_GEN = 1020;
    const float CURR_2_PH = 3. / 8;
    std::vector<std::vector<float>> pieds; /// Пьедесталы
    std::vector<float> curbase; /// Относительные токи
    std::vector<float> pulse; /// Импульсные характеристики тока
    std::vector<float> amp; /// Обратная функция распределения коэффициента усиления ФЭУ
    std::vector<int> Toff; /// Относительные сдвиги каналов
    std::vector<float> interf; /// Наводки
    std::vector<float> interf_amp; /// Покональные амплитудные коэффициенты наводки
    std::vector<int> thr; /// Пороги
    std::vector<float> Cal; /// Калибровка
    int N_PHEL{}; /// Число зарегистрированных фотонов
    std::vector<int> PMTid; /// Номера ФЭУ, куда упали фотоэлектроны
    std::vector<float> T; /// Времена прихода фотоэлектронов
    float Tmin{}; /// Минимальное время прихода
    float MEAN_CURR{3.5}; /// Средний ток
    std::vector<std::vector<int>> data_out; /// Выводной массив
    std::vector<std::vector<float>> data; /// Данные
public:
    ModelElectronics();

    void GetMoshits();

    void GetSimple(const std::string &, std::vector<float> &);

    void GetC();

    void GenerateEvent();

    void AddBackground();

    void SimulateDig();

    void PrintDataOut();

    std::vector<float> &getCurbaseRef() { return curbase; }

    std::vector<float> &getAmpRef() { return amp; }

    std::vector<float> &getPulseRef() { return pulse; }
};

ModelElectronics::ModelElectronics() :
        pieds(N_CHAN, std::vector<float>(2, 52.73)),
        curbase(N_CHAN, 0),
        pulse(PULSE_LENGTH, 0),
        amp(AMP_SIZE, 0),
        Cal(N_CHAN, 0),
        Toff(N_CHAN, 0),
        interf(INTERF_LENGTH, 0),
        interf_amp(N_CHAN, 0),
        thr(N_CHAN, 214),
        data_out(N_CHAN, std::vector<int>(BIN_2_GEN, 0)),
        data(N_CHAN, std::vector<float>(BIN_2_GEN * 25 + 2 * PULSE_LENGTH + 2, 0)) {}

/**
 * @brief Функция для считывания файла moshits
 */
void ModelElectronics::GetMoshits() {
    std::ifstream moshits("mosaic_hits_m01_pro_10PeV_10-20_001_c001");

    if (!moshits.is_open()) {
        std::cerr << "Failed to open the moshits file!" << std::endl;
    }
    N_PHEL = int(std::count(std::istreambuf_iterator<char>(moshits),
                            std::istreambuf_iterator<char>(), '\n')) - 1;

    moshits.clear();
    moshits.seekg(0, std::ios::beg);

    std::string line;
    std::getline(moshits, line);
    double pmt_tmp, t_tmp, tmp;
    while (std::getline(moshits, line)) {
        std::istringstream ss(line);
        ss >> pmt_tmp;
        for (int i{0}; i < 3; i++) {
            ss >> tmp;
        }
        ss >> t_tmp;
        PMTid.push_back(int(pmt_tmp));
        T.push_back(float(t_tmp));
    }
    auto min_it{std::min_element(T.begin(), T.end())};
    Tmin = *min_it;
    moshits.close();
}

/**
 * @brief Обобщенная функция для считывания файлов с одной строкой
 * @param fileName Имя файла
 * @param vec ссылка на массив, в который надо считать файл
 */
void ModelElectronics::GetSimple(const std::string &fileName, std::vector<float> &vec) {
    std::ifstream input(fileName);
    if (!input.is_open()) {
        std::cerr << "Failed to open the " << fileName << " file!" << std::endl;
    }
    std::string line;
    std::getline(input, line);
    std::istringstream ss(line);
    auto it{vec.begin()};
    float tmp;
    while (ss >> tmp && it != vec.end()) {
        *it = tmp;
        ++it;
    }
    input.close();
}

/**
 * @brief Функция для считывания файла калибровки
 */
void ModelElectronics::GetC() {
    std::ifstream cal("14484.cal");
    if (!cal.is_open()) {
        std::cerr << "Failed to open the calibration file!" << std::endl;
    }
    std::string line;
    float tmp;
    auto it{Cal.begin()};
    while (std::getline(cal, line) && it != Cal.end()) {
        std::istringstream ss(line);
        for (int i{0}; i < 13; i++) {
            ss >> tmp;
        }
        ss >> *it;
        ++it;
    }
    cal.close();
}

/**
 * @brief Генерация события
 */
void ModelElectronics::GenerateEvent() {
    int T_ph;
    float amp_ph;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, AMP_SIZE);
    for (int phid{0}; phid < N_PHEL; phid++) {
        amp_ph = amp[dist(gen)];
        T_ph = int(2 * (T[phid] - Tmin) + floor(0.45 * BIN_2_GEN * 25 + PULSE_LENGTH));
        for (int t{0}; t < PULSE_LENGTH; t++) {
            data[PMTid[phid]][t + T_ph] += amp_ph * pulse[t];
        }
    }
}

/**
 * @brief Добавление фона
 */
void ModelElectronics::AddBackground() {
    int BG_LENGTH{25 * BIN_2_GEN + PULSE_LENGTH + 25};
    float N_AVG;
    float N_PHEL_exp;
    float amp_ph;
    int T_ph;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_amp(0, AMP_SIZE);
    std::uniform_int_distribution<> dist_bg(0, BG_LENGTH);

    for (int j{0}; j < N_CHAN; j++) {
        N_AVG = MEAN_CURR * curbase[j] * CURR_2_PH;
        N_PHEL_exp = N_AVG * float(BG_LENGTH) / 25;
        std::poisson_distribution dist(N_PHEL_exp);
        N_PHEL = dist(gen);
        for (int n{0}; n < N_PHEL; n++) {
            amp_ph = amp[dist_amp(gen)];
            T_ph = dist_bg(gen);
            for (int t{0}; t < PULSE_LENGTH; t++) {
                data[j][t + T_ph] += amp_ph * pulse[t];
            }
        }
    }

}

/**
 * @brief Имитация оцифровки
 */
void ModelElectronics::SimulateDig() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_inter_length(0, INTERF_LENGTH);
    std::uniform_int_distribution<> dist_shift(0, 25);
    std::cout << "SimDig" << std::endl;
    int t_f{0};
    int t_shift{0};
    float S_avg{0};
    for (int j{0}; j < N_CHAN; j++) {
        t_shift = dist_inter_length(gen);
        t_shift = dist_shift(gen);
        for (int t{PULSE_LENGTH}; t < BIN_2_GEN * 25 + PULSE_LENGTH + 1; t++) {
            S_avg += data[j][t];
        }
        S_avg /= float(BIN_2_GEN) * 25;
        for (int i{0}; i < BIN_2_GEN; i++) {
            data_out[j][i] = int((data[j][PULSE_LENGTH + t_shift + i * 25 + Toff[j]] - S_avg) /
                                 Cal[j] + pieds[j][int(i % 2)] + pieds[j][int((i + 1) % 2)] +
                                  interf_amp[j] * interf[(i * 25 + Toff[j] + t_f) % INTERF_LENGTH]);
        }
    }

}

/**
 * @brief Метод для печати выводного файла
 */
void ModelElectronics::PrintDataOut() {
    std::ofstream outFile("data_out");
    if (!outFile.is_open()) {
        std::cerr << "Open file error." << std::endl;
    }
    std::vector<std::vector<int>> data_out_t = transpose(data_out);
    for (const auto &innerVec: data_out_t) {
        for (const auto &item: innerVec) {
            outFile << item << ' ';
        }
        outFile << std::endl;
    }
    outFile.close();
}

/**
 * @brief Класс для многопоточности
 */
class ThreadManager {
private:
    ModelElectronics &obj;
public:
    explicit ThreadManager(ModelElectronics &objRef) : obj(objRef) {}

    void inputAll();
};

/**
 * @brief Метод для заполнения массивов в многопоточном режиме
 */
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
    model.GenerateEvent();
    model.AddBackground();
    model.SimulateDig();
    model.PrintDataOut();
    return 0;
}
