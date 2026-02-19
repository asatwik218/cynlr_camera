#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>
extern "C" {
    #include <arv.h>
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static ArvGcNode* getBufferNode(ArvDevice* device) {
    ArvGc* genicam = arv_device_get_genicam(device);
    return arv_gc_get_node(genicam, "FileAccessBuffer");
}

// Poll until FileOperationStatus is no longer "Busy" (up to 2 seconds)
static bool checkStatus(ArvDevice* device) {
    GError* err = NULL;
    for (int i = 0; i < 200; i++) {
        const char* status = arv_device_get_string_feature_value(device, "FileOperationStatus", &err);
        if (err != NULL) {
            printf("    [status] Could not read FileOperationStatus: %s\n", err->message);
            g_clear_error(&err);
            return false;
        }
        if (status && strcmp(status, "Busy") == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        bool ok = (status != NULL && strcmp(status, "Success") == 0);
        printf("    [status] FileOperationStatus = %s\n", status ? status : "(null)");
        return ok;
    }
    printf("    [status] Timeout: still Busy after 2s\n");
    return false;
}

static gint64 getResult(ArvDevice* device) {
    GError* err = NULL;
    gint64 result = arv_device_get_integer_feature_value(device, "FileOperationResult", &err);
    if (err != NULL) {
        printf("    [result] Could not read FileOperationResult: %s\n", err->message);
        g_clear_error(&err);
        return -1;
    }
    printf("    [result] FileOperationResult = %lld bytes\n", (long long)result);
    return result;
}

// -----------------------------------------------------------------------
// Delete file
// -----------------------------------------------------------------------
static bool deleteFile(ArvDevice* device, const char* fileSelector) {
    GError* err = NULL;
    printf("  deleteFile: selector=%s\n", fileSelector);

    arv_device_set_string_feature_value(device, "FileSelector", fileSelector, &err);
    if (err) { printf("  ERR FileSelector: %s\n", err->message); g_clear_error(&err); return false; }

    arv_device_set_string_feature_value(device, "FileOperationSelector", "Delete", &err);
    if (err) { printf("  ERR FileOperationSelector=Delete: %s\n", err->message); g_clear_error(&err); return false; }

    arv_device_execute_command(device, "FileOperationExecute", &err);
    if (err) { printf("  ERR FileOperationExecute(Delete): %s\n", err->message); g_clear_error(&err); return false; }

    return checkStatus(device);
}

// -----------------------------------------------------------------------
// Open file
// -----------------------------------------------------------------------
static bool openFile(ArvDevice* device, const char* fileSelector, const char* mode) {
    GError* err = NULL;
    printf("  openFile: selector=%s mode=%s\n", fileSelector, mode);

    arv_device_set_string_feature_value(device, "FileSelector", fileSelector, &err);
    if (err) { printf("  ERR FileSelector: %s\n", err->message); g_clear_error(&err); return false; }

    arv_device_set_string_feature_value(device, "FileOpenMode", mode, &err);
    if (err) { printf("  ERR FileOpenMode: %s\n", err->message); g_clear_error(&err); return false; }

    arv_device_set_string_feature_value(device, "FileOperationSelector", "Open", &err);
    if (err) { printf("  ERR FileOperationSelector=Open: %s\n", err->message); g_clear_error(&err); return false; }

    arv_device_execute_command(device, "FileOperationExecute", &err);
    if (err) { printf("  ERR FileOperationExecute(Open): %s\n", err->message); g_clear_error(&err); return false; }

    return checkStatus(device);
}

// -----------------------------------------------------------------------
// Close file
// -----------------------------------------------------------------------
static bool closeFile(ArvDevice* device) {
    GError* err = NULL;
    printf("  closeFile\n");

    arv_device_set_string_feature_value(device, "FileOperationSelector", "Close", &err);
    if (err) { printf("  ERR FileOperationSelector=Close: %s\n", err->message); g_clear_error(&err); return false; }

    arv_device_execute_command(device, "FileOperationExecute", &err);
    if (err) { printf("  ERR FileOperationExecute(Close): %s\n", err->message); g_clear_error(&err); return false; }

    return checkStatus(device);
}

// -----------------------------------------------------------------------
// Write data to open file
// -----------------------------------------------------------------------
static int64_t writeData(ArvDevice* device, const void* data, size_t length) {
    GError* err = NULL;
    printf("  writeData: \"%.*s\" (%zu bytes)\n", (int)length, (const char*)data, length);

    arv_device_set_integer_feature_value(device, "FileAccessOffset", 0, &err);
    if (err) { printf("  ERR FileAccessOffset: %s\n", err->message); g_clear_error(&err); return -1; }

    // Get register size
    ArvGcNode* buf_node = getBufferNode(device);
    if (!buf_node) { printf("  ERR FileAccessBuffer node not found\n"); return -1; }

    guint64 reg_len = arv_gc_register_get_length(ARV_GC_REGISTER(buf_node), &err);
    if (err) { printf("  ERR get register length: %s\n", err->message); g_clear_error(&err); return -1; }
    printf("  FileAccessBuffer register size = %llu bytes\n", (unsigned long long)reg_len);

    if (length > reg_len) {
        printf("  ERR data (%zu) exceeds register size (%llu) — chunking needed\n",
               length, (unsigned long long)reg_len);
        return -1;
    }

    // FileAccessLength = actual data bytes
    arv_device_set_integer_feature_value(device, "FileAccessLength", (gint64)length, &err);
    if (err) { printf("  ERR FileAccessLength: %s\n", err->message); g_clear_error(&err); return -1; }

    // Set FileOperationSelector BEFORE writing the buffer — some cameras require this ordering
    arv_device_set_string_feature_value(device, "FileOperationSelector", "Write", &err);
    if (err) { printf("  ERR FileOperationSelector=Write: %s\n", err->message); g_clear_error(&err); return -1; }

    // Write directly at the physical address to bypass GenICam cache
    {
        GError* addr_err = NULL;
        guint64 reg_addr = arv_gc_register_get_address(ARV_GC_REGISTER(buf_node), &addr_err);
        if (addr_err) {
            printf("  WARN could not get register address: %s — falling back to gc_register_set\n", addr_err->message);
            g_clear_error(&addr_err);
            arv_gc_register_set(ARV_GC_REGISTER(buf_node), const_cast<void*>(data), length, &err);
        } else {
            printf("  FileAccessBuffer physical address = 0x%llX\n", (unsigned long long)reg_addr);
            arv_device_write_memory(device, reg_addr, length, const_cast<void*>(data), &err);
        }
        if (err) { printf("  ERR FileAccessBuffer write: %s\n", err->message); g_clear_error(&err); return -1; }
    }
    printf("  FileAccessBuffer set OK\n");

    arv_device_execute_command(device, "FileOperationExecute", &err);
    if (err) { printf("  ERR FileOperationExecute(Write): %s\n", err->message); g_clear_error(&err); return -1; }

    if (!checkStatus(device)) return -1;
    return getResult(device);
}

// -----------------------------------------------------------------------
// Read data from open file
// -----------------------------------------------------------------------
static int64_t readData(ArvDevice* device, void* buf, size_t length) {
    GError* err = NULL;
    printf("  readData: requesting %zu bytes\n", length);

    arv_device_set_integer_feature_value(device, "FileAccessOffset", 0, &err);
    if (err) { printf("  ERR FileAccessOffset: %s\n", err->message); g_clear_error(&err); return -1; }

    arv_device_set_integer_feature_value(device, "FileAccessLength", (gint64)length, &err);
    if (err) { printf("  ERR FileAccessLength: %s\n", err->message); g_clear_error(&err); return -1; }

    arv_device_set_string_feature_value(device, "FileOperationSelector", "Read", &err);
    if (err) { printf("  ERR FileOperationSelector=Read: %s\n", err->message); g_clear_error(&err); return -1; }

    arv_device_execute_command(device, "FileOperationExecute", &err);
    if (err) { printf("  ERR FileOperationExecute(Read): %s\n", err->message); g_clear_error(&err); return -1; }

    if (!checkStatus(device)) return -1;
    int64_t result = getResult(device);
    if (result <= 0) return result;

    ArvGcNode* buf_node = getBufferNode(device);
    if (!buf_node) { printf("  ERR FileAccessBuffer node not found\n"); return -1; }

    // arv_gc_register_get requires a buffer matching the full register size
    guint64 reg_len = arv_gc_register_get_length(ARV_GC_REGISTER(buf_node), &err);
    if (err) { printf("  ERR get register length: %s\n", err->message); g_clear_error(&err); return -1; }
    printf("  FileAccessBuffer register size = %llu bytes\n", (unsigned long long)reg_len);

    std::vector<uint8_t> reg_buf(reg_len, 0);
    arv_gc_register_get(ARV_GC_REGISTER(buf_node), reg_buf.data(), reg_len, &err);
    if (err) { printf("  ERR FileAccessBuffer read: %s\n", err->message); g_clear_error(&err); return -1; }

    // Copy only what was actually returned, capped to the caller's buffer size
    size_t copy_len = (size_t)result < length ? (size_t)result : length;
    memcpy(buf, reg_buf.data(), copy_len);
    return (int64_t)copy_len;
}

// -----------------------------------------------------------------------
// main
// -----------------------------------------------------------------------
int main() {
    printf("=== Aravis File Access Test ===\n\n");

    GError* err = NULL;
    ArvCamera* camera = arv_camera_new(NULL, &err);
    if (!ARV_IS_CAMERA(camera)) {
        printf("Failed to open camera\n");
        if (err) { printf("  Error: %s\n", err->message); g_clear_error(&err); }
        return -1;
    }
    printf("Camera opened: %s\n\n", arv_camera_get_model_name(camera, NULL));
    ArvDevice* device = arv_camera_get_device(camera);

    // ---- Test data ----
    const char* file_selector = "UserFile1";
    const char* write_str = "hello how are you";
    size_t write_len = strlen(write_str);

    printf("\n--- WRITE ---\n");
    if (!openFile(device, file_selector, "Write")) {
        printf("Failed to open file for writing\n");
        g_clear_object(&camera);
        return -1;
    }

    int64_t written = writeData(device, write_str, write_len);
    printf("  Bytes written: %lld\n", (long long)written);

    closeFile(device);

    if (written != (int64_t)write_len) {
        printf("FAIL: wrote %lld bytes, expected %zu\n", (long long)written, write_len);
        g_clear_object(&camera);
        return -1;
    }

    // Give camera time to commit write to flash before reading back
    printf("  sleeping 500ms for flash commit...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ----------------------------------------------------------------
    // READ BACK
    // ----------------------------------------------------------------
    printf("\n--- READ ---\n");
    if (!openFile(device, file_selector, "Read")) {
        printf("Failed to open file for reading\n");
        g_clear_object(&camera);
        return -1;
    }

    std::vector<uint8_t> read_buf(write_len + 1, 0);  // +1 for null terminator
    int64_t read_bytes = readData(device, read_buf.data(), write_len);
    printf("  Bytes read: %lld\n", (long long)read_bytes);

    closeFile(device);

    // ----------------------------------------------------------------
    // VERIFY
    // ----------------------------------------------------------------
    printf("\n--- VERIFY ---\n");
    // read_buf has write_len+1 elements; read_bytes <= write_len, so index is safe
    if (read_bytes > 0) read_buf[read_bytes] = '\0';
    printf("  Written : \"%s\"\n", write_str);
    printf("  Read    : \"%s\"\n", (char*)read_buf.data());
    if (read_bytes == (int64_t)write_len && memcmp(write_str, read_buf.data(), write_len) == 0) {
        printf("PASS: read back matches written data\n");
    } else {
        printf("FAIL: data mismatch\n");
    }

    g_clear_object(&camera);
    printf("\nDone.\n");
    return 0;
}
