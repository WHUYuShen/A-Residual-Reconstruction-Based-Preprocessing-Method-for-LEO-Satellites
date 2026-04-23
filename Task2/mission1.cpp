#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cctype>
#include <cstdlib>

using namespace std;



// ==================== 全局文件名（固定路径） ====================

string LOG_FILE = "D://xwechat_files//wxid_ku43fizhzl7l22_253f//msg//file//2025-11//grac1970.19o.log_orig";     // 输入 LOG
string OMC_FILE = "D://xwechat_files//wxid_ku43fizhzl7l22_253f//msg//file//2025-11//omc_2019197";               // 输入 OMC
string OUTPUT_FILE = "D://xwechat_files//wxid_ku43fizhzl7l22_253f//msg//file//2025-11//grac1970.19o_output";    // 输出 LOG

const double JUMP_THRESHOLD = 0.3; // 周跳阈值

struct OmcData {
    double jd;
    double sec;
    int epoch;
    string station;
    string prn;
    double phase_omc;
    double code_omc;
    double elev;
    double azim;
    int amb_flag;
};

struct LogRecord {
    string type;
    string prn;
    int start_epoch;
    int end_epoch;
    int flag;
    string comment;
    bool modified; // 标记是否被修改过

    // 备用排序：先按起始历元，再按PRN
    bool operator<(const LogRecord& other) const {
        if (start_epoch != other.start_epoch)
            return start_epoch < other.start_epoch;
        return prn < other.prn;
    }
};

// OMC 按 PRN + 测站分类：omcMap[prn][station] = vector<OmcData>
using OmcMap = map<string, map<string, vector<OmcData>>>;


// 从 LOG 文件名中推断测站名（例如 grad1970.19o.log_orig → GRAD）
string detectStationFromLogFilePath(const string& path) {
    size_t pos = path.find_last_of("\\/");
    size_t start = (pos == string::npos) ? 0 : pos + 1;

    string name;
    while (start < path.size() && isalpha(static_cast<unsigned char>(path[start]))) {
        name.push_back(static_cast<char>(toupper(static_cast<unsigned char>(path[start]))));
        start++;
    }
    return name;
}

bool isNumber(const string& s) {
    if (s.empty()) return false;
    char* endptr = nullptr;
    double val = strtod(s.c_str(), &endptr);
    (void)val; // 防止未使用警告
    return (*endptr == '\0');  // 全部成功解析为 double
}


// ==================== 读取 OMC 文件 ====================

OmcMap readOmcData(set<string>& stations) {
    OmcMap omcMap;
    ifstream fin(OMC_FILE);
    string line;

    if (!fin) {
        cerr << "错误：无法打开OMC文件 " << OMC_FILE << endl;
        exit(1);
    }

    while (getline(fin, line)) {
        if (line.empty()) continue;

        // 去掉行首空格方便判断
        size_t first = line.find_first_not_of(" \t");
        if (first == string::npos) continue;
        if (line[first] == '+') continue; // +omc 之类的行直接跳过

        // 拆分成 token
        istringstream issTokens(line);
        vector<string> tokens;
        string tok;
        while (issTokens >> tok) {
            tokens.push_back(tok);
        }
        if (tokens.empty()) continue;

        // 1) 识别测站名称行：GRAC -xxx -xxx xxx
        if (tokens.size() >= 4 &&
            !isdigit(static_cast<unsigned char>(tokens[0][0])) &&
            isNumber(tokens[1]) && isNumber(tokens[2]) && isNumber(tokens[3])) {

            stations.insert(tokens[0]);   // GRAC / GRAD
            continue;                     // 这是头信息，不是观测数据
        }

        // 2) 识别真正的 OMC 数据行
        // 格式：jd sec station prn phase_omc code_omc elev azim amb_flag ...
        if (tokens.size() < 9 || !isNumber(tokens[0]) || !isNumber(tokens[1])) {
            // 既不是 station 行也不是数据行（比如 PRN 列表、"2 31 10.0..." 等），跳过
            continue;
        }

        OmcData data;
        istringstream iss(line);
        iss >> data.jd >> data.sec >> data.station >> data.prn
            >> data.phase_omc >> data.code_omc >> data.elev
            >> data.azim >> data.amb_flag;

        data.epoch = static_cast<int>(data.sec / 10) + 1;

        omcMap[data.prn][data.station].push_back(data);
    }

    return omcMap;
}

// ===== 单历元最大模糊度检查（详情） =====
void computeMaxAmbDetail(
    const vector<LogRecord>& records,
    int& maxAmb,
    vector<int>& maxEpochs,
    map<int, vector<LogRecord>>& epochRecords
) {
    map<int, int> countMap;

    // 遍历所有 AMB 区间
    for (const auto& rec : records) {
        if (rec.type != "AMB") continue;

        for (int e = rec.start_epoch; e <= rec.end_epoch; ++e) {
            countMap[e]++;
            epochRecords[e].push_back(rec);   // 将此行加入对应历元
        }
    }

    // 找最大次数
    maxAmb = 0;
    for (auto& p : countMap) {
        maxAmb = max(maxAmb, p.second);
    }

    // 找所有达到最大值的历元
    for (auto& p : countMap) {
        if (p.second == maxAmb) {
            maxEpochs.push_back(p.first);
        }
    }
}


// ==================== 读取 LOG 文件 ====================

vector<LogRecord> readLogRecords(vector<string>& logHeader) {
    vector<LogRecord> records;
    ifstream fin(LOG_FILE);

    if (!fin) {
        cerr << "错误：无法打开LOG文件 " << LOG_FILE << endl;
        exit(1);
    }

    string line;
    bool inHeader = true;

    while (getline(fin, line)) {
        // 先处理头部
        if (inHeader) {
            logHeader.push_back(line);  // 把头部原样保存下来

            if (line.find("%End of header") != string::npos) {
                inHeader = false;
            }
            continue;  // 头部行不再往下做解析
        }

        // 数据区（AMB / DEL 等）
        if (line.length() < 3) continue;

        string type = line.substr(0, 3);
        if (type != "DEL" && type != "AMB") continue;

        LogRecord rec;
        istringstream iss(line);

        iss >> rec.type >> rec.prn >> rec.start_epoch
            >> rec.end_epoch >> rec.flag;

        size_t pos = line.find("RN_");
        rec.comment = (pos != string::npos) ? line.substr(pos) : "";
        rec.modified = false; // 初始化为未修改

        records.push_back(rec);
    }

    fin.close();
    return records;
}


// ==================== 各种辅助函数 ====================

vector<int> detectJumps(const vector<OmcData>& data, int start, int end) {
    vector<int> jumps;
    if (data.empty()) return jumps;

    int start_idx = 0;
    while (start_idx < static_cast<int>(data.size()) && data[start_idx].epoch < start) {
        start_idx++;
    }

    int end_idx = static_cast<int>(data.size()) - 1;
    while (end_idx >= 0 && data[end_idx].epoch > end) {
        end_idx--;
    }

    if (start_idx > end_idx) return jumps;

    double prev = data[start_idx].phase_omc;
    for (int i = start_idx + 1; i <= end_idx; i++) {
        double diff = fabs(data[i].phase_omc - prev);
        if (diff > JUMP_THRESHOLD) {
            jumps.push_back(data[i].epoch);
        }
        prev = data[i].phase_omc;
    }

    return jumps;
}

set<int> getOmcEpochsForPrn(const vector<OmcData>& omcData) {
    set<int> epochs;
    for (const auto& data : omcData) {
        epochs.insert(data.epoch);
    }
    return epochs;
}

vector<pair<int, int>> findMissingEpochs(const set<int>& omcEpochs, int start, int end) {
    vector<pair<int, int>> missing;
    if (omcEpochs.empty()) {
        missing.emplace_back(start, end);
        return missing;
    }

    int current = start;
    for (int epoch : omcEpochs) {
        if (epoch < start) continue;
        if (epoch > end) break;

        if (epoch > current) {
            missing.emplace_back(current, epoch - 1);
        }
        current = epoch + 1;
    }

    if (current <= end) {
        missing.emplace_back(current, end);
    }

    return missing;
}


// ==================== 合并相邻记录（暂未使用，可保留） ====================

vector<LogRecord> mergeRecordsWithTypeCheck(vector<LogRecord> records) {
    if (records.empty()) return records;

    vector<LogRecord> merged;
    merged.push_back(records[0]);

    for (size_t i = 1; i < records.size(); ++i) {
        LogRecord& last = merged.back();
        if (last.prn == records[i].prn &&
            last.type == records[i].type &&
            last.flag == records[i].flag &&
            last.modified == records[i].modified &&
            last.end_epoch + 1 == records[i].start_epoch) {
            last.end_epoch = records[i].end_epoch;
            if (last.comment.find("RN_checked") != string::npos) {
                last.comment = records[i].comment;
            }
        }
        else {
            merged.push_back(records[i]);
        }
    }

    return merged;
}


// ==================== 主处理逻辑 ====================

vector<LogRecord> processRecords(const vector<LogRecord>& records,
    const OmcMap& omcMap,
    const string& stationFilter) {

    vector<LogRecord> output;

    // 1. 为每个 PRN（仅当前测站）创建 OMC 历元集合
    map<string, set<int>> prnOmcEpochs;
    for (const auto& [prn, stationMap] : omcMap) {
        auto sit = stationMap.find(stationFilter);
        if (sit == stationMap.end()) continue;
        prnOmcEpochs[prn] = getOmcEpochsForPrn(sit->second);
    }

    // 2. 处理原始 LOG 记录
    for (const auto& rec : records) {
        if (rec.start_epoch <= 0 || rec.end_epoch <= 0 || rec.start_epoch > rec.end_epoch) {
            cerr << "警告：跳过无效 epoch 范围的记录: PRN=" << rec.prn
                << ", start=" << rec.start_epoch << ", end=" << rec.end_epoch << endl;
            continue;
        }

        const string& prn = rec.prn;

        auto prnIt = omcMap.find(prn);
        auto epochIt = prnOmcEpochs.find(prn);

        if (prnIt == omcMap.end() || epochIt == prnOmcEpochs.end()) {
            LogRecord delRec = rec;
            delRec.type = "DEL";
            delRec.flag = 6;
            delRec.comment = "RN_checked";
            delRec.modified = true;
            output.push_back(delRec);
            continue;
        }

        auto stationIt = prnIt->second.find(stationFilter);
        if (stationIt == prnIt->second.end()) {
            LogRecord delRec = rec;
            delRec.type = "DEL";
            delRec.flag = 6;
            delRec.comment = "RN_checked";
            delRec.modified = true;
            output.push_back(delRec);
            continue;
        }

        const auto& omcData = stationIt->second;
        const auto& omcEpochs = epochIt->second;

        auto missingEpochs = findMissingEpochs(omcEpochs, rec.start_epoch, rec.end_epoch);

        if (missingEpochs.empty()) {
            vector<int> jumps = detectJumps(omcData, rec.start_epoch, rec.end_epoch);
            if (jumps.empty()) {
                if (rec.type == "DEL") {
                    LogRecord newRec = rec;
                    newRec.type = "AMB";
                    newRec.flag = 1;
                    newRec.comment = "RN_checked";
                    newRec.modified = true;
                    output.push_back(newRec);
                }
                else {
                    output.push_back(rec);
                }
            }
            else {
                int current = rec.start_epoch;
                for (int jump : jumps) {
                    if (jump > current) {
                        LogRecord seg = rec;
                        seg.start_epoch = current;
                        seg.end_epoch = jump - 1;
                        seg.type = "AMB";
                        seg.flag = 1;
                        seg.comment = "RN_checked";
                        seg.modified = true;
                        output.push_back(seg);
                        current = jump;
                    }
                }
                if (current <= rec.end_epoch) {
                    LogRecord seg = rec;
                    seg.start_epoch = current;
                    seg.end_epoch = rec.end_epoch;
                    seg.type = "AMB";
                    seg.flag = 1;
                    seg.comment = "RN_checked";
                    seg.modified = true;
                    output.push_back(seg);
                }
            }
        }
        else {
            int current = rec.start_epoch;
            for (const auto& me : missingEpochs) {
                int missingStart = me.first;
                int missingEnd = me.second;

                if (missingStart > current) {
                    vector<int> jumps = detectJumps(omcData, current, missingStart - 1);
                    if (jumps.empty()) {
                        LogRecord seg = rec;
                        seg.start_epoch = current;
                        seg.end_epoch = missingStart - 1;
                        seg.type = "AMB";
                        seg.flag = 1;
                        seg.comment = "RN_checked";
                        seg.modified = true;
                        output.push_back(seg);
                    }
                    else {
                        int segStart = current;
                        for (int jump : jumps) {
                            if (jump > segStart) {
                                LogRecord seg = rec;
                                seg.start_epoch = segStart;
                                seg.end_epoch = jump - 1;
                                seg.type = "AMB";
                                seg.flag = 1;
                                seg.comment = "RN_checked";
                                seg.modified = true;
                                output.push_back(seg);
                                segStart = jump;
                            }
                        }
                        if (segStart <= missingStart - 1) {
                            LogRecord seg = rec;
                            seg.start_epoch = segStart;
                            seg.end_epoch = missingStart - 1;
                            seg.type = "AMB";
                            seg.flag = 1;
                            seg.comment = "RN_checked";
                            seg.modified = true;
                            output.push_back(seg);
                        }
                    }
                }

                LogRecord gapRec = rec;
                gapRec.type = "DEL";
                gapRec.start_epoch = missingStart;
                gapRec.end_epoch = missingEnd;
                gapRec.flag = 6;
                gapRec.comment = "RN_checked";
                gapRec.modified = true;
                output.push_back(gapRec);

                current = missingEnd + 1;
            }

            if (current <= rec.end_epoch) {
                vector<int> jumps = detectJumps(omcData, current, rec.end_epoch);
                if (jumps.empty()) {
                    LogRecord seg = rec;
                    seg.start_epoch = current;
                    seg.end_epoch = rec.end_epoch;
                    seg.type = "AMB";
                    seg.flag = 1;
                    seg.comment = "RN_checked";
                    seg.modified = true;
                    output.push_back(seg);
                }
                else {
                    int segStart = current;
                    for (int jump : jumps) {
                        if (jump > segStart) {
                            LogRecord seg = rec;
                            seg.start_epoch = segStart;
                            seg.end_epoch = jump - 1;
                            seg.type = "AMB";
                            seg.flag = 1;
                            seg.comment = "RN_checked";
                            seg.modified = true;
                            output.push_back(seg);
                            segStart = jump;
                        }
                    }
                    if (segStart <= rec.end_epoch) {
                        LogRecord seg = rec;
                        seg.start_epoch = segStart;
                        seg.end_epoch = rec.end_epoch;
                        seg.type = "AMB";
                        seg.flag = 1;
                        seg.comment = "RN_checked";
                        seg.modified = true;
                        output.push_back(seg);
                    }
                }
            }
        }
    }

    // 3. OMC 中有但 LOG 中未记录的卫星数据（仅当前测站）
    for (const auto& prnPair : omcMap) {
        const string& prn = prnPair.first;
        auto stationIt = prnPair.second.find(stationFilter);
        if (stationIt == prnPair.second.end()) continue;

        const auto& omcData = stationIt->second;
        if (omcData.empty()) continue;

        set<pair<int, int>> loggedEpochs;
        for (const auto& rec : output) {
            if (rec.prn == prn) {
                loggedEpochs.insert({ rec.start_epoch, rec.end_epoch });
            }
        }

        int start = omcData.front().epoch;
        int end = omcData.back().epoch;
        vector<pair<int, int>> missingRanges;

        if (loggedEpochs.empty()) {
            missingRanges.emplace_back(start, end);
        }
        else {
            int current = start;
            for (const auto& se : loggedEpochs) {
                int s = se.first;
                int e = se.second;
                if (s > current) {
                    missingRanges.emplace_back(current, s - 1);
                }
                current = max(current, e + 1);
            }
            if (current <= end) {
                missingRanges.emplace_back(current, end);
            }
        }

        for (const auto& se : missingRanges) {
            int s = se.first;
            int e = se.second;

            set<int> actualOmcEpochs;
            for (const auto& data : omcData) {
                if (data.epoch >= s && data.epoch <= e) {
                    actualOmcEpochs.insert(data.epoch);
                }
            }

            if (actualOmcEpochs.empty()) {
                LogRecord delRec;
                delRec.type = "DEL";
                delRec.prn = prn;
                delRec.start_epoch = s;
                delRec.end_epoch = e;
                delRec.flag = 6;
                delRec.comment = "RN_checked";
                delRec.modified = true;
                output.push_back(delRec);
            }
            else {
                vector<int> jumps = detectJumps(omcData, s, e);
                if (jumps.empty()) {
                    LogRecord ambRec;
                    ambRec.type = "AMB";
                    ambRec.prn = prn;
                    ambRec.start_epoch = s;
                    ambRec.end_epoch = e;
                    ambRec.flag = 1;
                    ambRec.comment = "RN_checked";
                    ambRec.modified = true;
                    output.push_back(ambRec);
                }
                else {
                    int current = s;
                    for (int jump : jumps) {
                        if (jump > current) {
                            LogRecord seg;
                            seg.type = "AMB";
                            seg.prn = prn;
                            seg.start_epoch = current;
                            seg.end_epoch = jump - 1;
                            seg.flag = 1;
                            seg.comment = "RN_checked";
                            seg.modified = true;
                            output.push_back(seg);
                            current = jump;
                        }
                    }
                    if (current <= e) {
                        LogRecord seg;
                        seg.type = "AMB";
                        seg.prn = prn;
                        seg.start_epoch = current;
                        seg.end_epoch = e;
                        seg.flag = 1;
                        seg.comment = "RN_checked";
                        seg.modified = true;
                        output.push_back(seg);
                    }
                }
            }
        }
    }

    return output;
}

// 最多模糊度历元统计（仅用于写头部）
int computeMaxAmbInOneEpoch(const vector<LogRecord>& records) {
    map<int, int> epochCount;

    for (const auto& rec : records) {
        if (rec.type != "AMB") continue;

        for (int e = rec.start_epoch; e <= rec.end_epoch; ++e) {
            epochCount[e]++;
        }
    }

    int maxCount = 0;
    for (const auto& p : epochCount) {
        maxCount = max(maxCount, p.second);
    }
    return maxCount;
}


// ==================== 写输出文件 ====================

void writeOutput(const vector<LogRecord>& records, vector<string> logHeader) {
    ofstream fout(OUTPUT_FILE);
    if (!fout) {
        cerr << "错误：无法创建输出文件 " << OUTPUT_FILE << endl;
        exit(1);
    }

    // 统计 AMB 行数
    int ambCount = 0;
    for (const auto& rec : records) {
        if (rec.type == "AMB") ++ambCount;
    }

    // 统计单个历元最多模糊度出现次数
    int maxAmb = computeMaxAmbInOneEpoch(records);

    // 更新头部
    for (string& line : logHeader) {

        // 更新 Existed ambiguities 行（注意：这里你加了一个 %，要和实际头一致）
        if (line.find("Existed") != string::npos &&
            line.find("ambiguities") != string::npos) {

            ostringstream oss;
            oss << "%Existed    ambiguities  : " << setw(10) << ambCount;
            line = oss.str();
        }

        // 更新 %Max ambc in one epoch 行
        if (line.find("%Max ambc in one epoch") != string::npos) {
            ostringstream oss;
            oss << "%Max ambc in one epoch   : " << setw(10) << maxAmb;
            line = oss.str();
        }

        fout << line << "\n";
    }

    fout << "\n";

    // 输出数据部分
    for (const auto& rec : records) {
        if (rec.start_epoch <= 0 || rec.end_epoch <= 0 || rec.prn.empty()) continue;

        fout << setw(3) << left << rec.type << " "
            << setw(4) << rec.prn << " "
            << setw(7) << rec.start_epoch << " "
            << setw(7) << rec.end_epoch << " "
            << setw(5) << rec.flag << " "
            << setw(18) << 0.0 << " "
            << setw(18) << 0.0 << " ";

        if (rec.modified) {
            fout << "RN_checked";
        }
        else {
            fout << (rec.comment.empty() ? "" : rec.comment);
        }
        fout << "\n";
    }
}


// ==================== main：使用命令行参数 ====================

// ==================== main：使用固定文件路径 ====================

int main() {
    cout << "=== OMC数据更新程序 ===" << endl;
    cout << "输入 LOG 文件 : " << LOG_FILE << endl;
    cout << "输入 OMC 文件 : " << OMC_FILE << endl;
    cout << "输出 LOG 文件 : " << OUTPUT_FILE << endl;

    // 读取 OMC，统计测站种类
    set<string> stations;
    OmcMap omcData = readOmcData(stations);

    cout << "OMC 中检测到的测站种类: ";
    for (const auto& s : stations) cout << s << " ";
    cout << endl;

    // 根据 LOG 文件名推断当前测站
    string stationFilter = detectStationFromLogFilePath(LOG_FILE);
    if (stationFilter.empty()) {
        cerr << "警告：无法从LOG文件名推断测站，程序可能无法正确区分测站！" << endl;
    }
    else {
        cout << "推断当前LOG对应测站为: " << stationFilter << endl;
    }

    vector<string> logHeader;
    auto logRecords = readLogRecords(logHeader);

    auto processed = processRecords(logRecords, omcData, stationFilter);

    // 清除非法记录
    processed.erase(
        remove_if(processed.begin(), processed.end(), [](const LogRecord& rec) {
            return rec.start_epoch <= 0 || rec.end_epoch <= 0 || rec.prn.empty();
            }),
        processed.end()
    );

    // 按 start_epoch 排序
    sort(processed.begin(), processed.end(), [](const LogRecord& a, const LogRecord& b) {
        return a.start_epoch < b.start_epoch;
        });

    writeOutput(processed, logHeader);

    cout << "处理完成！共处理 " << logRecords.size() << " 条原始记录" << endl;
    cout << "生成 " << processed.size() << " 条输出记录" << endl;

    // ===== 单历元最大模糊度统计（控制台检查用） =====
    int maxAmb = 0;
    vector<int> maxEpochs;
    map<int, vector<LogRecord>> epochRecords;

    computeMaxAmbDetail(processed, maxAmb, maxEpochs, epochRecords);

    cout << "\n=== 单历元最大模糊度统计 ===\n";
    cout << "最大模糊度数量: " << maxAmb << endl;

    cout << "出现该最大模糊度的历元: ";
    for (int e : maxEpochs) cout << e << " ";
    cout << "\n\n";

    for (int e : maxEpochs) {
        cout << "--- 历元 " << e << " 对应的 AMB 记录 ---\n";
        for (const auto& rec : epochRecords[e]) {
            cout << rec.type << " " << rec.prn << " "
                << rec.start_epoch << " " << rec.end_epoch << " "
                << rec.flag << " " << rec.comment << "\n";
        }
        cout << "\n";
    }

    return 0;
}
