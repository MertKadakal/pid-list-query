#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <ncurses.h>
#include <unistd.h> // usleep için
#include <vector>
#include <string>
#include <algorithm>


struct ProcessNode {
    int pid;
    int ppid;
    std::string name;
    std::vector<ProcessNode*> children;
};



// Mevcut ProcessNode ve yardımcı fonksiyonlarını burada tutabilirsin...

void start_monitoring() {
    // ncurses başlatma
    initscr();            // Ekranı başlat
    noecho();             // Yazılan tuşları ekrana basma
    curs_set(0);          // İmleci gizle
    timeout(1000);        // getch() için 1 saniye bekle (1000ms), sonra devam et

    int row, col;
    while (true) {
        getmaxyx(stdscr, row, col); // Terminal boyutlarını al
        erase();                    // Ekranı temizle

        // Başlık kısmı
        attron(A_BOLD | A_REVERSE);
        mvprintw(0, 0, "%-10s | %-20s | %-15s | %-10s", "PID", "Proses Adı", "Durum", "PPID");
        attroff(A_BOLD | A_REVERSE);

        // /proc dizininden gerçek süreç verilerini oku
        int current_row = 1;
        DIR *procdir = opendir("/proc");
        if (procdir) {
            struct dirent *entry;
            while ((entry = readdir(procdir)) && current_row < row - 1) {
                // Sadece PID dizinlerini kontrol et
                if (!is_pid_dir(entry)) continue;

                char path[300];
                snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
                FILE *fp = fopen(path, "r");
                if (!fp) continue;

                int pid, ppid;
                char comm[256];
                char state;

                if (fscanf(fp, "%d (%[^)]) %c %d", &pid, comm, &state, &ppid) == 4) {
                    mvprintw(current_row++, 0, "%-10d | %-20s | %-15s | %-10d",
                             pid, comm, getStateName(state).c_str(), ppid);
                }
                fclose(fp);
            }
            closedir(procdir);
        }

        mvprintw(row - 1, 0, "Çıkmak için 'q' tuşuna basın... (Her 1 sn güncellenir)");
        refresh(); // Değişiklikleri ekrana yansıt

        // Kullanıcı girdisi kontrolü
        int ch = getch();
        if (ch == 'q') break;
    }

    endwin(); // ncurses'ı kapat ve terminali eski haline getir
}

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

    for (ProcessNode* child : node->children) {
        cleanup(child);
    }

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
        // Sadece ihtiyac olan PID, Name ve PPID'yi al
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
        std::cout << "\n===== MENÜ ====="  << std::endl;
        std::cout << "1. PID detayını görüntüle"  << std::endl;
        std::cout << "2. Canlı süreç izleme (ncurses)"  << std::endl;
        std::cout << "3. Çıkış"  << std::endl;
        std::cout << "Seçiminiz: ";

        int choice;
        std::cin >> choice;

        if (choice == 3) {
            break;
        } else if (choice == 2) {
            start_monitoring();
            continue;
        } else if (choice == 1) {
            std::cout << "Stat içeriğini görmek istediğiniz PID'yi girin: ";
            int pid;
            std::cin >> pid;

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
        } else {
            std::cout << "Geçersiz seçim. Lütfen tekrar deneyin." << std::endl;
        }
    }

    return 0;
}