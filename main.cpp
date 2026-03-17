#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>

// Eksik olan C++ kütüphaneleri:
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

struct ProcessNode {
    int pid;
    int ppid;
    std::string name;
    std::vector<ProcessNode*> children;
};

// PID dizini mi kontrol et
int is_pid_dir(const struct dirent *entry) {
    for (const char *p = entry->d_name; *p; p++) {
        if (!isdigit(*p)) return 0;
    }
    return 1;
}

// Ağacı recursive olarak ekrana bas
void printTree(ProcessNode* node, int depth = 0, bool isLast = true) {
    if (!node) return;

    for (int i = 0; i < depth; ++i) {
        std::cout << "    ";
    }
    
    std::cout << (isLast ? "└── " : "├── ") << node->name << " [" << node->pid << "]" << std::endl;

    for (size_t i = 0; i < node->children.size(); ++i) {
        printTree(node->children[i], depth + 1, i == node->children.size() - 1);
    }
}

void cleanup(ProcessNode* node) {
    if (!node) return;

    // Önce çocukları temizle (Post-order traversal)
    for (ProcessNode* child : node->children) {
        cleanup(child);
    }

    // Sonra kendini sil
    delete node;
}

std::string getStateName(char state) {
    switch (state) {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Disk Sleep";
        case 'Z': return "Zombie";
        case 'T': return "Stopped";
        default: return "Unknown (" + std::to_string(state) + ")";
    }
}

int main() {
    DIR *procdir = opendir("/proc");
    if (!procdir) {
        perror("opendir failed");
        return 1;
    }

    std::unordered_map<int, ProcessNode*> all_processes;
    struct dirent *entry;

    // 1. ADIM: Tüm süreçleri tara ve Node nesnelerini oluştur
    while ((entry = readdir(procdir))) {
        if (!is_pid_dir(entry)) continue;

        char path[300];
        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        int pid, ppid;
        char comm[256];
        // Sadece ihtiyacımız olan PID, Name ve PPID'yi alıyoruz
        if (fscanf(fp, "%d (%[^)]) %*c %d", &pid, comm, &ppid) == 3) {
            ProcessNode* node = new ProcessNode();
            node->pid = pid;
            node->ppid = ppid;
            node->name = comm;
            all_processes[pid] = node;
        }
        fclose(fp);
    }
    closedir(procdir);

    // 2. ADIM: Ebeveyn-Çocuk ilişkilerini kur
    ProcessNode* root = nullptr;
    for (auto const& [pid, node] : all_processes) {
        if (all_processes.count(node->ppid) && node->ppid != node->pid) {
            all_processes[node->ppid]->children.push_back(node);
        } else {
            // PID 1 genellikle root'tur (systemd)
            if (node->pid == 1) root = node;
        }
    }

    // 3. ADIM: Yazdır
    if (root) {
        std::cout << "Sistem Süreç Ağacı:" << std::endl;
        printTree(root);
    }

    // 4. ADIM: Tüm PID'lerin state bilgisini listele
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TÜM SÜREÇLERİN STATE BİLGİSİ:" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "PID       | Proses Adı           | Durum           | Parent PID" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    procdir = opendir("/proc");
    if (procdir) {
        while ((entry = readdir(procdir))) {
            if (!is_pid_dir(entry)) continue;

            char path[300];
            snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
            FILE *fp = fopen(path, "r");
            if (!fp) continue;

            int pid, ppid;
            char comm[256];
            char state;
            
            if (fscanf(fp, "%d (%[^)]) %c %d", &pid, comm, &state, &ppid) == 4) {
                printf("%-9d | %-20s | %-15s | %d\n", 
                       pid, comm, getStateName(state).c_str(), ppid);
            }
            fclose(fp);
        }
        closedir(procdir);
    }
    std::cout << std::string(70, '=') << std::endl;

    if (root) {
        cleanup(root);
        std::cout << "\n[Bilgi] Bellek başarıyla temizlendi." << std::endl;
    }

    while (true) {
        std::cout << "\nStat içeriğini görmek istediğiniz PID'yi girin (Çıkmak için -1): ";
        int pid;
        std::cin >> pid;
        if (pid == -1) {
            break;
        }

        char path[300];
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        
        FILE *fp = fopen(path, "r");
        if (!fp) {
            std::cerr << "Hata: PID " << pid << " için stat dosyası açılamadı." << std::endl;
            continue;
        }

        std::cout << "\n=== /proc/" << pid << "/stat İçeriği ===" << std::endl;
        
        // Stat dosyasının tüm içeriğini oku ve yazdır
        char buffer[2048];
        if (fgets(buffer, sizeof(buffer), fp)) {
            std::cout << buffer << std::endl;
        }
        
        std::cout << "\n=== Ayrıntılı Analiz ===" << std::endl;
        rewind(fp);
        
        // Dosyayı tekrar aç ve ayrıntılı bilgisini göster
        int tmp_pid, ppid, pgrp, session;
        char comm[256];
        char state;
        unsigned long utime, stime, vsize;
        long priority, nice;
        unsigned int rt_priority, policy;
        
        
        if (fscanf(fp, "%d (%[^)]) %c %d %d %d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %*d %*d %*d %*d %u %lu %u %lu",
                   &tmp_pid, comm, &state, &ppid, &pgrp, &session, &utime, &stime, &priority, &nice, &rt_priority, &vsize) >= 7) {
            std::cout << "PID: " << tmp_pid << std::endl;
            std::cout << "Proses Adı: " << comm << std::endl;
            std::cout << "Durum: " << getStateName(state) << std::endl;
            std::cout << "Parent PID: " << ppid << std::endl;
            std::cout << "Process Group: " << pgrp << std::endl;
            std::cout << "Session ID: " << session << std::endl;
            std::cout << "User Time: " << utime << " jiffies" << std::endl;
            std::cout << "System Time: " << stime << " jiffies" << std::endl;
            std::cout << "Priority: " << priority << std::endl;
            std::cout << "Nice: " << nice << std::endl;
            std::cout << "Virtual Memory: " << vsize << " bytes" << std::endl;
        }
        fclose(fp);
    }

    return 0;
}