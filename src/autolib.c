#include "autolib.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *const *libs;
    size_t lib_count;
    const char *const *headers;
    size_t header_count;
    const char *const *identifiers;
    size_t identifier_count;
    const char *const *prefixes;
    size_t prefix_count;
} AutoLibRule;

static const char *M_LIBS[] = {"m"};
static const char *M_HEADERS[] = {"math.h"};
static const char *M_IDENTIFIERS[] = {
    "sqrt", "sqrtf", "sqrtl", "sin", "sinf", "sinl", "cos", "cosf", "cosl", "tan", "tanf", "tanl", "pow",
    "powf", "powl", "floor", "floorf", "floorl", "ceil", "ceilf", "ceill", "round", "roundf", "roundl", "exp",
    "expf", "log", "logf", "log10", "log10f", "fmod", "fmodf", "atan2", "atan2f", "hypot", "hypotf", "cbrt", "cbrtf"
};

static const char *PTHREAD_LIBS[] = {"pthread"};
static const char *PTHREAD_HEADERS[] = {"pthread.h", "semaphore.h"};
static const char *PTHREAD_IDENTIFIERS[] = {"pthread_create", "pthread_join", "sem_init", "sem_wait", "sem_post"};
static const char *PTHREAD_PREFIXES[] = {"pthread_mutex_", "pthread_cond_", "pthread_rwlock_", "pthread_spin_",
                                         "pthread_barrier_", "sem_"};

static const char *DL_LIBS[] = {"dl"};
static const char *DL_HEADERS[] = {"dlfcn.h"};
static const char *DL_IDENTIFIERS[] = {"dlopen", "dlsym", "dlclose", "dlerror", "dladdr"};

static const char *RT_LIBS[] = {"rt"};
static const char *RT_HEADERS[] = {"aio.h", "mqueue.h", "time.h"};
static const char *RT_IDENTIFIERS[] = {"clock_gettime", "clock_settime", "timer_create", "timer_settime", "mq_open",
                                       "mq_send", "mq_receive", "mq_close", "shm_open", "aio_read", "aio_write"};
static const char *RT_PREFIXES[] = {"timer_", "mq_"};

static const char *CURL_LIBS[] = {"curl"};
static const char *CURL_HEADERS[] = {"curl/curl.h"};
static const char *CURL_IDENTIFIERS[] = {"curl_easy_init", "curl_easy_perform", "curl_multi_init", "curl_url"};
static const char *CURL_PREFIXES[] = {"curl_easy_", "curl_multi_", "curl_url_", "curl_ws_"};

static const char *OPENSSL_LIBS[] = {"ssl", "crypto"};
static const char *OPENSSL_HEADERS[] = {"openssl/ssl.h", "openssl/evp.h", "openssl/bio.h", "openssl/x509.h",
                                        "openssl/err.h", "openssl/rand.h"};
static const char *OPENSSL_IDENTIFIERS[] = {"SSL_library_init", "SSL_CTX_new", "EVP_EncryptInit_ex",
                                            "OPENSSL_init_ssl", "ERR_get_error"};
static const char *OPENSSL_PREFIXES[] = {"SSL_", "EVP_", "BIO_", "X509_", "ERR_", "OPENSSL_", "RAND_"};

static const char *ZLIB_LIBS[] = {"z"};
static const char *ZLIB_HEADERS[] = {"zlib.h"};
static const char *ZLIB_IDENTIFIERS[] = {"deflateInit", "inflateInit", "compress", "uncompress", "gzopen", "gzread"};
static const char *ZLIB_PREFIXES[] = {"deflate", "inflate", "gz"};

static const char *PNG_LIBS[] = {"png"};
static const char *PNG_HEADERS[] = {"png.h"};
static const char *PNG_IDENTIFIERS[] = {"png_create_read_struct", "png_create_write_struct", "png_read_info",
                                        "png_write_info"};
static const char *PNG_PREFIXES[] = {"png_"};

static const char *JPEG_LIBS[] = {"jpeg"};
static const char *JPEG_HEADERS[] = {"jpeglib.h"};
static const char *JPEG_IDENTIFIERS[] = {"jpeg_create_decompress", "jpeg_create_compress", "jpeg_read_header",
                                         "jpeg_start_decompress", "jpeg_write_scanlines"};
static const char *JPEG_PREFIXES[] = {"jpeg_"};

static const char *XML2_LIBS[] = {"xml2"};
static const char *XML2_HEADERS[] = {"libxml/parser.h", "libxml/tree.h", "libxml/xmlreader.h"};
static const char *XML2_IDENTIFIERS[] = {"xmlReadFile", "xmlReadMemory", "xmlParseFile", "xmlDocGetRootElement",
                                         "xmlFreeDoc"};
static const char *XML2_PREFIXES[] = {"xmlRead", "xmlParse", "xmlDoc", "xmlNode", "xmlFree"};

static const char *YAML_LIBS[] = {"yaml"};
static const char *YAML_HEADERS[] = {"yaml.h"};
static const char *YAML_IDENTIFIERS[] = {"yaml_parser_initialize", "yaml_emitter_initialize", "yaml_parser_parse"};
static const char *YAML_PREFIXES[] = {"yaml_"};

static const char *CJSON_LIBS[] = {"cjson"};
static const char *CJSON_HEADERS[] = {"cJSON.h", "cjson/cJSON.h"};
static const char *CJSON_IDENTIFIERS[] = {"cJSON_Parse", "cJSON_Print", "cJSON_CreateObject"};
static const char *CJSON_PREFIXES[] = {"cJSON_"};

static const char *JANSSON_LIBS[] = {"jansson"};
static const char *JANSSON_HEADERS[] = {"jansson.h"};
static const char *JANSSON_IDENTIFIERS[] = {"json_loads", "json_load_file", "json_dumps", "json_object", "json_array"};

static const char *SQLITE_LIBS[] = {"sqlite3"};
static const char *SQLITE_HEADERS[] = {"sqlite3.h"};
static const char *SQLITE_IDENTIFIERS[] = {"sqlite3_open", "sqlite3_exec", "sqlite3_prepare_v2"};
static const char *SQLITE_PREFIXES[] = {"sqlite3_"};

static const char *GIT2_LIBS[] = {"git2"};
static const char *GIT2_HEADERS[] = {"git2.h", "git2/repository.h", "git2/clone.h"};
static const char *GIT2_IDENTIFIERS[] = {"git_libgit2_init", "git_repository_open", "git_clone"};
static const char *GIT2_PREFIXES[] = {"git_repository_", "git_commit_", "git_remote_", "git_reference_", "git_blob_",
                                      "git_tree_", "git_index_", "git_tag_"};

static const char *UV_LIBS[] = {"uv"};
static const char *UV_HEADERS[] = {"uv.h"};
static const char *UV_IDENTIFIERS[] = {"uv_loop_init", "uv_run", "uv_tcp_init"};
static const char *UV_PREFIXES[] = {"uv_"};

static const char *GLIB_LIBS[] = {"glib-2.0"};
static const char *GLIB_HEADERS[] = {"glib.h", "glib/gprintf.h", "glib/gstdio.h"};
static const char *GLIB_IDENTIFIERS[] = {"g_malloc", "g_free", "g_hash_table_new", "g_main_loop_new",
                                         "g_string_new"};
static const char *GLIB_PREFIXES[] = {"g_list_", "g_hash_table_", "g_string_", "g_array_", "g_queue_",
                                      "g_main_loop_"};

static const char *GOBJECT_LIBS[] = {"gobject-2.0", "glib-2.0"};
static const char *GOBJECT_HEADERS[] = {"glib-object.h", "gobject/gobject.h"};
static const char *GOBJECT_IDENTIFIERS[] = {"g_object_new", "g_signal_connect_data", "g_type_class_ref"};
static const char *GOBJECT_PREFIXES[] = {"g_object_", "g_signal_", "g_type_", "g_param_spec_"};

static const char *NCURSES_LIBS[] = {"ncurses"};
static const char *NCURSES_HEADERS[] = {"curses.h", "ncurses.h"};
static const char *NCURSES_IDENTIFIERS[] = {"initscr", "newwin", "wgetch", "endwin", "cbreak", "noecho"};

static const char *READLINE_LIBS[] = {"readline"};
static const char *READLINE_HEADERS[] = {"readline/readline.h", "readline/history.h"};
static const char *READLINE_IDENTIFIERS[] = {"readline", "add_history", "rl_bind_key"};
static const char *READLINE_PREFIXES[] = {"rl_"};

static const char *SDL2_LIBS[] = {"SDL2"};
static const char *SDL2_HEADERS[] = {"SDL2/SDL.h", "SDL2/SDL_image.h", "SDL2/SDL_mixer.h", "SDL2/SDL_ttf.h"};
static const char *SDL2_IDENTIFIERS[] = {"SDL_Init", "SDL_CreateWindow", "SDL_PollEvent", "SDL_RenderPresent"};
static const char *SDL2_PREFIXES[] = {"SDL_"};

static const char *ALLEGRO_LIBS[] = {"allegro"};
static const char *ALLEGRO_HEADERS[] = {"allegro5/allegro.h", "allegro5/allegro_image.h",
                                        "allegro5/allegro_audio.h"};
static const char *ALLEGRO_IDENTIFIERS[] = {"al_init", "al_create_display", "al_flip_display",
                                            "al_install_keyboard"};
static const char *ALLEGRO_PREFIXES[] = {"al_"};

static const char *RAYLIB_LIBS[] = {"raylib"};
static const char *RAYLIB_HEADERS[] = {"raylib.h"};
static const char *RAYLIB_IDENTIFIERS[] = {"InitWindow", "BeginDrawing", "EndDrawing", "CloseWindow", "DrawText"};

static const char *OPENAL_LIBS[] = {"openal"};
static const char *OPENAL_HEADERS[] = {"AL/al.h", "AL/alc.h"};
static const char *OPENAL_IDENTIFIERS[] = {"alGenBuffers", "alSourcePlay", "alcOpenDevice", "alcCreateContext"};
static const char *OPENAL_PREFIXES[] = {"alBuffer", "alSource", "alc"};

static const char *PORTAUDIO_LIBS[] = {"portaudio"};
static const char *PORTAUDIO_HEADERS[] = {"portaudio.h"};
static const char *PORTAUDIO_IDENTIFIERS[] = {"Pa_Initialize", "Pa_OpenStream", "Pa_StartStream"};
static const char *PORTAUDIO_PREFIXES[] = {"Pa_"};

static const char *PULSE_LIBS[] = {"pulse"};
static const char *PULSE_HEADERS[] = {"pulse/pulseaudio.h"};
static const char *PULSE_IDENTIFIERS[] = {"pa_context_new", "pa_mainloop_new", "pa_stream_new"};
static const char *PULSE_PREFIXES[] = {"pa_context_", "pa_mainloop_", "pa_stream_"};

static const char *PULSE_SIMPLE_LIBS[] = {"pulse", "pulse-simple"};
static const char *PULSE_SIMPLE_HEADERS[] = {"pulse/simple.h"};
static const char *PULSE_SIMPLE_IDENTIFIERS[] = {"pa_simple_new", "pa_simple_write", "pa_simple_read"};
static const char *PULSE_SIMPLE_PREFIXES[] = {"pa_simple_"};

static const char *PIPEWIRE_LIBS[] = {"pipewire-0.3"};
static const char *PIPEWIRE_HEADERS[] = {"pipewire/pipewire.h"};
static const char *PIPEWIRE_IDENTIFIERS[] = {"pw_init", "pw_main_loop_new", "pw_context_new"};
static const char *PIPEWIRE_PREFIXES[] = {"pw_"};

static const char *EVENT_LIBS[] = {"event"};
static const char *EVENT_HEADERS[] = {"event2/event.h", "event2/buffer.h", "event2/bufferevent.h"};
static const char *EVENT_IDENTIFIERS[] = {"event_base_new", "event_new", "event_add", "event_dispatch",
                                          "evbuffer_new", "bufferevent_socket_new"};
static const char *EVENT_PREFIXES[] = {"evbuffer_", "bufferevent_", "evconnlistener_"};

static const char *EV_LIBS[] = {"ev"};
static const char *EV_HEADERS[] = {"ev.h"};
static const char *EV_IDENTIFIERS[] = {"ev_default_loop", "ev_run", "ev_io_init", "ev_timer_init"};
static const char *EV_PREFIXES[] = {"ev_io_", "ev_timer_", "ev_async_"};

static const char *FFI_LIBS[] = {"ffi"};
static const char *FFI_HEADERS[] = {"ffi.h"};
static const char *FFI_IDENTIFIERS[] = {"ffi_prep_cif", "ffi_call", "ffi_prep_closure_loc"};
static const char *FFI_PREFIXES[] = {"ffi_"};

static const char *ARCHIVE_LIBS[] = {"archive"};
static const char *ARCHIVE_HEADERS[] = {"archive.h", "archive_entry.h"};
static const char *ARCHIVE_IDENTIFIERS[] = {"archive_read_new", "archive_write_new", "archive_read_next_header"};
static const char *ARCHIVE_PREFIXES[] = {"archive_"};

static const char *ZIP_LIBS[] = {"zip"};
static const char *ZIP_HEADERS[] = {"zip.h"};
static const char *ZIP_IDENTIFIERS[] = {"zip_open", "zip_close", "zip_fopen"};
static const char *ZIP_PREFIXES[] = {"zip_"};

static const char *BZ2_LIBS[] = {"bz2"};
static const char *BZ2_HEADERS[] = {"bzlib.h"};
static const char *BZ2_IDENTIFIERS[] = {"BZ2_bzCompressInit", "BZ2_bzDecompressInit", "BZ2_bzReadOpen"};
static const char *BZ2_PREFIXES[] = {"BZ2_"};

static const char *LZMA_LIBS[] = {"lzma"};
static const char *LZMA_HEADERS[] = {"lzma.h"};
static const char *LZMA_IDENTIFIERS[] = {"lzma_easy_encoder", "lzma_stream_decoder", "lzma_code"};
static const char *LZMA_PREFIXES[] = {"lzma_"};

static const char *PROTOBUFC_LIBS[] = {"protobuf-c"};
static const char *PROTOBUFC_HEADERS[] = {"protobuf-c/protobuf-c.h"};
static const char *PROTOBUFC_IDENTIFIERS[] = {"protobuf_c_message_pack", "protobuf_c_message_unpack"};
static const char *PROTOBUFC_PREFIXES[] = {"protobuf_c_"};

static const char *NANOMSG_LIBS[] = {"nanomsg"};
static const char *NANOMSG_HEADERS[] = {"nanomsg/nn.h", "nanomsg/pubsub.h", "nanomsg/reqrep.h"};
static const char *NANOMSG_IDENTIFIERS[] = {"nn_socket", "nn_bind", "nn_connect", "nn_send", "nn_recv"};
static const char *NANOMSG_PREFIXES[] = {"nn_"};

static const char *NNG_LIBS[] = {"nng"};
static const char *NNG_HEADERS[] = {"nng/nng.h", "nng/protocol/pubsub0/pub.h", "nng/protocol/reqrep0/req.h"};
static const char *NNG_IDENTIFIERS[] = {"nng_socket_open", "nng_listen", "nng_send", "nng_recv"};
static const char *NNG_PREFIXES[] = {"nng_"};

static const char *ZMQ_LIBS[] = {"zmq"};
static const char *ZMQ_HEADERS[] = {"zmq.h"};
static const char *ZMQ_IDENTIFIERS[] = {"zmq_ctx_new", "zmq_socket", "zmq_bind", "zmq_connect"};
static const char *ZMQ_PREFIXES[] = {"zmq_"};

static const char *HIREDIS_LIBS[] = {"hiredis"};
static const char *HIREDIS_HEADERS[] = {"hiredis/hiredis.h", "hiredis/async.h"};
static const char *HIREDIS_IDENTIFIERS[] = {"redisConnect", "redisCommand", "redisFree"};
static const char *HIREDIS_PREFIXES[] = {"redis"};

static const char *PQ_LIBS[] = {"pq"};
static const char *PQ_HEADERS[] = {"libpq-fe.h"};
static const char *PQ_IDENTIFIERS[] = {"PQconnectdb", "PQexec", "PQfinish"};
static const char *PQ_PREFIXES[] = {"PQ"};

static const char *MYSQL_LIBS[] = {"mysqlclient"};
static const char *MYSQL_HEADERS[] = {"mysql/mysql.h"};
static const char *MYSQL_IDENTIFIERS[] = {"mysql_init", "mysql_real_connect", "mysql_query"};
static const char *MYSQL_PREFIXES[] = {"mysql_"};

static const char *MONGOC_LIBS[] = {"mongoc-1.0", "bson-1.0"};
static const char *MONGOC_HEADERS[] = {"mongoc/mongoc.h", "bson/bson.h"};
static const char *MONGOC_IDENTIFIERS[] = {"mongoc_init", "mongoc_client_new", "bson_new"};
static const char *MONGOC_PREFIXES[] = {"mongoc_", "bson_"};

static const char *CAIRO_LIBS[] = {"cairo"};
static const char *CAIRO_HEADERS[] = {"cairo.h", "cairo/cairo.h"};
static const char *CAIRO_IDENTIFIERS[] = {"cairo_create", "cairo_move_to", "cairo_show_text"};
static const char *CAIRO_PREFIXES[] = {"cairo_"};

static const char *PANGO_LIBS[] = {"pango-1.0"};
static const char *PANGO_HEADERS[] = {"pango/pango.h", "pango/pangocairo.h"};
static const char *PANGO_IDENTIFIERS[] = {"pango_layout_new", "pango_layout_set_text",
                                          "pango_font_description_from_string"};
static const char *PANGO_PREFIXES[] = {"pango_"};

static const char *FREETYPE_LIBS[] = {"freetype"};
static const char *FREETYPE_HEADERS[] = {"ft2build.h", "freetype/freetype.h"};
static const char *FREETYPE_IDENTIFIERS[] = {"FT_Init_FreeType", "FT_New_Face", "FT_Load_Char"};
static const char *FREETYPE_PREFIXES[] = {"FT_"};

static const char *HARFBUZZ_LIBS[] = {"harfbuzz"};
static const char *HARFBUZZ_HEADERS[] = {"hb.h", "harfbuzz/hb.h"};
static const char *HARFBUZZ_IDENTIFIERS[] = {"hb_buffer_create", "hb_shape", "hb_font_create"};
static const char *HARFBUZZ_PREFIXES[] = {"hb_"};

static const char *DRM_LIBS[] = {"drm"};
static const char *DRM_HEADERS[] = {"xf86drm.h", "xf86drmMode.h"};
static const char *DRM_IDENTIFIERS[] = {"drmModeGetResources", "drmModeSetCrtc", "drmGetDevices2"};
static const char *DRM_PREFIXES[] = {"drmMode", "drmGet", "drmSet"};

static const char *VULKAN_LIBS[] = {"vulkan"};
static const char *VULKAN_HEADERS[] = {"vulkan/vulkan.h"};
static const char *VULKAN_IDENTIFIERS[] = {"vkCreateInstance", "vkEnumeratePhysicalDevices", "vkCreateDevice"};
static const char *VULKAN_PREFIXES[] = {"vk"};

static const char *GLEW_LIBS[] = {"GLEW"};
static const char *GLEW_HEADERS[] = {"GL/glew.h"};
static const char *GLEW_IDENTIFIERS[] = {"glewInit", "glewIsSupported"};
static const char *GLEW_PREFIXES[] = {"glew"};

static const char *LIBUSB_LIBS[] = {"usb-1.0"};
static const char *LIBUSB_HEADERS[] = {"libusb.h", "libusb-1.0/libusb.h"};
static const char *LIBUSB_IDENTIFIERS[] = {"libusb_init", "libusb_open_device_with_vid_pid", "libusb_handle_events"};
static const char *LIBUSB_PREFIXES[] = {"libusb_"};

static const char *PCAP_LIBS[] = {"pcap"};
static const char *PCAP_HEADERS[] = {"pcap.h"};
static const char *PCAP_IDENTIFIERS[] = {"pcap_open_live", "pcap_loop", "pcap_compile", "pcap_next_ex"};
static const char *PCAP_PREFIXES[] = {"pcap_"};

static const char *UUID_LIBS[] = {"uuid"};
static const char *UUID_HEADERS[] = {"uuid/uuid.h"};
static const char *UUID_IDENTIFIERS[] = {"uuid_generate", "uuid_parse", "uuid_unparse"};
static const char *UUID_PREFIXES[] = {"uuid_"};

static const char *BLKID_LIBS[] = {"blkid"};
static const char *BLKID_HEADERS[] = {"blkid/blkid.h"};
static const char *BLKID_IDENTIFIERS[] = {"blkid_new_probe", "blkid_do_safeprobe", "blkid_probe_lookup_value"};
static const char *BLKID_PREFIXES[] = {"blkid_"};

static const char *MOUNT_LIBS[] = {"mount"};
static const char *MOUNT_HEADERS[] = {"libmount/libmount.h"};
static const char *MOUNT_IDENTIFIERS[] = {"mnt_new_context", "mnt_context_mount", "mnt_table_parse_file"};
static const char *MOUNT_PREFIXES[] = {"mnt_"};

static const char *SECCOMP_LIBS[] = {"seccomp"};
static const char *SECCOMP_HEADERS[] = {"seccomp.h"};
static const char *SECCOMP_IDENTIFIERS[] = {"seccomp_init", "seccomp_rule_add", "seccomp_load"};
static const char *SECCOMP_PREFIXES[] = {"seccomp_"};

static const char *CAP_LIBS[] = {"cap"};
static const char *CAP_HEADERS[] = {"sys/capability.h"};
static const char *CAP_IDENTIFIERS[] = {"cap_get_proc", "cap_set_proc", "cap_from_text"};
static const char *CAP_PREFIXES[] = {"cap_"};

static const char *SYSTEMD_LIBS[] = {"systemd"};
static const char *SYSTEMD_HEADERS[] = {"systemd/sd-daemon.h", "systemd/sd-journal.h", "systemd/sd-bus.h"};
static const char *SYSTEMD_IDENTIFIERS[] = {"sd_notify", "sd_journal_send", "sd_bus_open_system"};
static const char *SYSTEMD_PREFIXES[] = {"sd_"};

static const char *UDEV_LIBS[] = {"udev"};
static const char *UDEV_HEADERS[] = {"libudev.h"};
static const char *UDEV_IDENTIFIERS[] = {"udev_new", "udev_monitor_new_from_netlink", "udev_device_get_devnode"};
static const char *UDEV_PREFIXES[] = {"udev_"};

static const char *PCI_LIBS[] = {"pci"};
static const char *PCI_HEADERS[] = {"pci/pci.h"};
static const char *PCI_IDENTIFIERS[] = {"pci_alloc", "pci_init", "pci_scan_bus"};
static const char *PCI_PREFIXES[] = {"pci_"};

static const char *LIBNL_LIBS[] = {"nl-3", "nl-route-3"};
static const char *LIBNL_HEADERS[] = {"netlink/netlink.h", "netlink/route/link.h", "netlink/route/route.h"};
static const char *LIBNL_IDENTIFIERS[] = {"nl_socket_alloc", "nl_connect", "rtnl_link_alloc"};
static const char *LIBNL_PREFIXES[] = {"nl_", "rtnl_", "genl_"};

static const char *LIBNET_LIBS[] = {"net"};
static const char *LIBNET_HEADERS[] = {"libnet.h"};
static const char *LIBNET_IDENTIFIERS[] = {"libnet_init", "libnet_write", "libnet_build_ipv4"};
static const char *LIBNET_PREFIXES[] = {"libnet_"};

static const char *SSH_LIBS[] = {"ssh"};
static const char *SSH_HEADERS[] = {"libssh/libssh.h", "libssh/callbacks.h"};
static const char *SSH_IDENTIFIERS[] = {"ssh_new", "ssh_connect", "ssh_channel_new"};
static const char *SSH_PREFIXES[] = {"ssh_"};

static const char *SSH2_LIBS[] = {"ssh2"};
static const char *SSH2_HEADERS[] = {"libssh2.h"};
static const char *SSH2_IDENTIFIERS[] = {"libssh2_init", "libssh2_session_init_ex", "libssh2_channel_open_ex"};
static const char *SSH2_PREFIXES[] = {"libssh2_"};

static const char *GCRYPT_LIBS[] = {"gcrypt"};
static const char *GCRYPT_HEADERS[] = {"gcrypt.h"};
static const char *GCRYPT_IDENTIFIERS[] = {"gcry_check_version", "gcry_cipher_open", "gcry_md_open"};
static const char *GCRYPT_PREFIXES[] = {"gcry_"};

static const char *TASN1_LIBS[] = {"tasn1"};
static const char *TASN1_HEADERS[] = {"libtasn1.h"};
static const char *TASN1_IDENTIFIERS[] = {"asn1_array2tree", "asn1_read_value", "asn1_create_element"};
static const char *TASN1_PREFIXES[] = {"asn1_"};

static const char *NETTLE_LIBS[] = {"nettle"};
static const char *NETTLE_HEADERS[] = {"nettle/aes.h", "nettle/sha2.h", "nettle/hmac.h"};
static const char *NETTLE_IDENTIFIERS[] = {"nettle_aes_set_encrypt_key", "nettle_sha256_init", "nettle_hmac_sha256_set_key"};
static const char *NETTLE_PREFIXES[] = {"nettle_"};

static const char *GNUTLS_LIBS[] = {"gnutls"};
static const char *GNUTLS_HEADERS[] = {"gnutls/gnutls.h"};
static const char *GNUTLS_IDENTIFIERS[] = {"gnutls_global_init", "gnutls_handshake", "gnutls_record_send"};
static const char *GNUTLS_PREFIXES[] = {"gnutls_"};

static const char *MBEDTLS_LIBS[] = {"mbedtls", "mbedx509", "mbedcrypto"};
static const char *MBEDTLS_HEADERS[] = {"mbedtls/ssl.h", "mbedtls/ctr_drbg.h", "mbedtls/x509_crt.h"};
static const char *MBEDTLS_IDENTIFIERS[] = {"mbedtls_ssl_init", "mbedtls_ctr_drbg_init", "mbedtls_x509_crt_init"};
static const char *MBEDTLS_PREFIXES[] = {"mbedtls_"};

static const char *WOLFSSL_LIBS[] = {"wolfssl"};
static const char *WOLFSSL_HEADERS[] = {"wolfssl/ssl.h", "wolfssl/options.h"};
static const char *WOLFSSL_IDENTIFIERS[] = {"wolfSSL_Init", "wolfSSL_new", "wolfSSL_CTX_new"};
static const char *WOLFSSL_PREFIXES[] = {"wolfSSL_"};

static const char *SODIUM_LIBS[] = {"sodium"};
static const char *SODIUM_HEADERS[] = {"sodium.h"};
static const char *SODIUM_IDENTIFIERS[] = {"sodium_init", "crypto_secretbox_easy", "crypto_box_keypair"};

static const char *ARGON2_LIBS[] = {"argon2"};
static const char *ARGON2_HEADERS[] = {"argon2.h"};
static const char *ARGON2_IDENTIFIERS[] = {"argon2_hash", "argon2id_hash_encoded", "argon2_verify"};
static const char *ARGON2_PREFIXES[] = {"argon2_"};

static const char *BCRYPT_LIBS[] = {"bcrypt"};
static const char *BCRYPT_HEADERS[] = {"bcrypt.h"};
static const char *BCRYPT_IDENTIFIERS[] = {"bcrypt_hashpw", "bcrypt_gensalt", "bcrypt_checkpw"};
static const char *BCRYPT_PREFIXES[] = {"bcrypt_"};

static const char *EDIT_LIBS[] = {"edit"};
static const char *EDIT_HEADERS[] = {"histedit.h"};
static const char *EDIT_IDENTIFIERS[] = {"el_init", "el_gets", "history_init"};

static const char *CONFIG_LIBS[] = {"config"};
static const char *CONFIG_HEADERS[] = {"libconfig.h"};
static const char *CONFIG_IDENTIFIERS[] = {"config_init", "config_read_file", "config_lookup_string"};
static const char *CONFIG_PREFIXES[] = {"config_"};

static const char *INIPARSER_LIBS[] = {"iniparser"};
static const char *INIPARSER_HEADERS[] = {"iniparser.h", "dictionary.h"};
static const char *INIPARSER_IDENTIFIERS[] = {"iniparser_load", "iniparser_getstring", "dictionary_new"};
static const char *INIPARSER_PREFIXES[] = {"iniparser_", "dictionary_"};

static const char *TOML_LIBS[] = {"toml"};
static const char *TOML_HEADERS[] = {"toml.h"};
static const char *TOML_IDENTIFIERS[] = {"toml_parse_file", "toml_parse", "toml_table_in"};
static const char *TOML_PREFIXES[] = {"toml_"};

static const char *CIVETWEB_LIBS[] = {"civetweb"};
static const char *CIVETWEB_HEADERS[] = {"civetweb.h"};
static const char *CIVETWEB_IDENTIFIERS[] = {"mg_start", "mg_stop", "mg_set_request_handler"};

static const char *MONGOOSE_LIBS[] = {"mongoose"};
static const char *MONGOOSE_HEADERS[] = {"mongoose.h"};
static const char *MONGOOSE_IDENTIFIERS[] = {"mg_mgr_init", "mg_http_listen", "mg_mgr_poll"};

static const char *MHD_LIBS[] = {"microhttpd"};
static const char *MHD_HEADERS[] = {"microhttpd.h"};
static const char *MHD_IDENTIFIERS[] = {"MHD_start_daemon", "MHD_create_response_from_buffer", "MHD_queue_response"};
static const char *MHD_PREFIXES[] = {"MHD_"};

static const char *WEBSOCKETS_LIBS[] = {"websockets"};
static const char *WEBSOCKETS_HEADERS[] = {"libwebsockets.h"};
static const char *WEBSOCKETS_IDENTIFIERS[] = {"lws_create_context", "lws_write", "lws_callback_on_writable"};
static const char *WEBSOCKETS_PREFIXES[] = {"lws_"};

static const char *RABBITMQ_LIBS[] = {"rabbitmq"};
static const char *RABBITMQ_HEADERS[] = {"amqp.h", "amqp_tcp_socket.h"};
static const char *RABBITMQ_IDENTIFIERS[] = {"amqp_new_connection", "amqp_basic_publish", "amqp_login"};
static const char *RABBITMQ_PREFIXES[] = {"amqp_"};

static const char *RDKAFKA_LIBS[] = {"rdkafka"};
static const char *RDKAFKA_HEADERS[] = {"rdkafka.h", "librdkafka/rdkafka.h"};
static const char *RDKAFKA_IDENTIFIERS[] = {"rd_kafka_new", "rd_kafka_poll", "rd_kafka_brokers_add"};
static const char *RDKAFKA_PREFIXES[] = {"rd_kafka_"};

static const char *MODBUS_LIBS[] = {"modbus"};
static const char *MODBUS_HEADERS[] = {"modbus.h"};
static const char *MODBUS_IDENTIFIERS[] = {"modbus_new_tcp", "modbus_connect", "modbus_read_registers"};
static const char *MODBUS_PREFIXES[] = {"modbus_"};

static const char *SERIALPORT_LIBS[] = {"serialport"};
static const char *SERIALPORT_HEADERS[] = {"libserialport.h"};
static const char *SERIALPORT_IDENTIFIERS[] = {"sp_get_port_by_name", "sp_open", "sp_blocking_read"};
static const char *SERIALPORT_PREFIXES[] = {"sp_"};

static const char *FTDI_LIBS[] = {"ftdi1"};
static const char *FTDI_HEADERS[] = {"libftdi1/ftdi.h"};
static const char *FTDI_IDENTIFIERS[] = {"ftdi_new", "ftdi_usb_open", "ftdi_read_data"};
static const char *FTDI_PREFIXES[] = {"ftdi_"};

static const char *GPIOD_LIBS[] = {"gpiod"};
static const char *GPIOD_HEADERS[] = {"gpiod.h"};
static const char *GPIOD_IDENTIFIERS[] = {"gpiod_chip_open", "gpiod_line_request_output", "gpiod_line_set_value"};
static const char *GPIOD_PREFIXES[] = {"gpiod_"};

static const char *WIRINGPI_LIBS[] = {"wiringPi"};
static const char *WIRINGPI_HEADERS[] = {"wiringPi.h", "softPwm.h", "softTone.h"};
static const char *WIRINGPI_IDENTIFIERS[] = {"wiringPiSetup", "pinMode", "digitalWrite", "softPwmCreate"};

static const char *LIBINPUT_LIBS[] = {"input"};
static const char *LIBINPUT_HEADERS[] = {"libinput.h"};
static const char *LIBINPUT_IDENTIFIERS[] = {"libinput_path_create_context", "libinput_dispatch",
                                             "libinput_get_event"};
static const char *LIBINPUT_PREFIXES[] = {"libinput_"};

static const char *WAYLAND_CLIENT_LIBS[] = {"wayland-client"};
static const char *WAYLAND_CLIENT_HEADERS[] = {"wayland-client.h"};
static const char *WAYLAND_CLIENT_IDENTIFIERS[] = {"wl_display_connect", "wl_display_dispatch", "wl_registry_bind"};
static const char *WAYLAND_CLIENT_PREFIXES[] = {"wl_display_", "wl_registry_", "wl_proxy_"};

static const char *WAYLAND_SERVER_LIBS[] = {"wayland-server"};
static const char *WAYLAND_SERVER_HEADERS[] = {"wayland-server-core.h", "wayland-server.h"};
static const char *WAYLAND_SERVER_IDENTIFIERS[] = {"wl_display_create", "wl_event_loop_dispatch", "wl_client_create"};
static const char *WAYLAND_SERVER_PREFIXES[] = {"wl_event_loop_", "wl_client_"};

static const char *XKBCOMMON_LIBS[] = {"xkbcommon"};
static const char *XKBCOMMON_HEADERS[] = {"xkbcommon/xkbcommon.h"};
static const char *XKBCOMMON_IDENTIFIERS[] = {"xkb_context_new", "xkb_keymap_new_from_string", "xkb_state_new"};
static const char *XKBCOMMON_PREFIXES[] = {"xkb_"};

static const char *XCB_LIBS[] = {"xcb"};
static const char *XCB_HEADERS[] = {"xcb/xcb.h"};
static const char *XCB_IDENTIFIERS[] = {"xcb_connect", "xcb_create_window", "xcb_wait_for_event"};
static const char *XCB_PREFIXES[] = {"xcb_"};

static const char *X11_LIBS[] = {"X11"};
static const char *X11_HEADERS[] = {"X11/Xlib.h", "X11/Xutil.h"};
static const char *X11_IDENTIFIERS[] = {"XOpenDisplay", "XCreateWindow", "XNextEvent", "XMapWindow"};


/* Additional auto-lib rules requested in issue follow-up. */
static const char *AUTO_LIBOGG_LIBS[] = {"ogg"};
static const char *AUTO_LIBOGG_HEADERS[] = {"ogg/ogg.h"};
static const char *AUTO_LIBOGG_IDENTIFIERS[] = {"ogg_sync_init"};

static const char *AUTO_LIBVORBIS_LIBS[] = {"vorbis"};
static const char *AUTO_LIBVORBIS_HEADERS[] = {"vorbis/vorbisfile.h"};
static const char *AUTO_LIBVORBIS_IDENTIFIERS[] = {"vorbis_info_init"};

static const char *AUTO_LIBTHEORA_LIBS[] = {"theora"};
static const char *AUTO_LIBTHEORA_HEADERS[] = {"theora/theoradec.h"};
static const char *AUTO_LIBTHEORA_IDENTIFIERS[] = {"th_decode_alloc"};

static const char *AUTO_LIBOPUS_LIBS[] = {"opus"};
static const char *AUTO_LIBOPUS_HEADERS[] = {"opus/opus.h"};
static const char *AUTO_LIBOPUS_IDENTIFIERS[] = {"opus_decoder_create"};

static const char *AUTO_OPUSFILE_LIBS[] = {"opusfile"};
static const char *AUTO_OPUSFILE_HEADERS[] = {"opus/opusfile.h"};
static const char *AUTO_OPUSFILE_IDENTIFIERS[] = {"op_open_file"};

static const char *AUTO_LIBFLAC_LIBS[] = {"FLAC"};
static const char *AUTO_LIBFLAC_HEADERS[] = {"FLAC/stream_decoder.h"};
static const char *AUTO_LIBFLAC_IDENTIFIERS[] = {"FLAC__stream_decoder_new"};

static const char *AUTO_LIBSNDFILE_LIBS[] = {"sndfile"};
static const char *AUTO_LIBSNDFILE_HEADERS[] = {"sndfile.h"};
static const char *AUTO_LIBSNDFILE_IDENTIFIERS[] = {"sf_open"};

static const char *AUTO_MPG123_LIBS[] = {"mpg123"};
static const char *AUTO_MPG123_HEADERS[] = {"mpg123.h"};
static const char *AUTO_MPG123_IDENTIFIERS[] = {"mpg123_init"};

static const char *AUTO_LIBMPG123_LIBS[] = {"mpg123"};
static const char *AUTO_LIBMPG123_HEADERS[] = {"mpg123.h"};
static const char *AUTO_LIBMPG123_IDENTIFIERS[] = {"mpg123_read"};

static const char *AUTO_LAME_LIBS[] = {"lame"};
static const char *AUTO_LAME_HEADERS[] = {"lame/lame.h"};
static const char *AUTO_LAME_IDENTIFIERS[] = {"lame_init"};

static const char *AUTO_LIBAVCODEC_LIBS[] = {"avcodec"};
static const char *AUTO_LIBAVCODEC_HEADERS[] = {"libavcodec/avcodec.h"};
static const char *AUTO_LIBAVCODEC_IDENTIFIERS[] = {"avcodec_find_decoder"};

static const char *AUTO_LIBAVFORMAT_LIBS[] = {"avformat"};
static const char *AUTO_LIBAVFORMAT_HEADERS[] = {"libavformat/avformat.h"};
static const char *AUTO_LIBAVFORMAT_IDENTIFIERS[] = {"avformat_open_input"};

static const char *AUTO_LIBAVUTIL_LIBS[] = {"avutil"};
static const char *AUTO_LIBAVUTIL_HEADERS[] = {"libavutil/avutil.h"};
static const char *AUTO_LIBAVUTIL_IDENTIFIERS[] = {"av_log_set_level"};

static const char *AUTO_LIBSWSCALE_LIBS[] = {"swscale"};
static const char *AUTO_LIBSWSCALE_HEADERS[] = {"libswscale/swscale.h"};
static const char *AUTO_LIBSWSCALE_IDENTIFIERS[] = {"sws_getContext"};

static const char *AUTO_LIBSWRESAMPLE_LIBS[] = {"swresample"};
static const char *AUTO_LIBSWRESAMPLE_HEADERS[] = {"libswresample/swresample.h"};
static const char *AUTO_LIBSWRESAMPLE_IDENTIFIERS[] = {"swr_alloc"};

static const char *AUTO_LIBPOSTPROC_LIBS[] = {"postproc"};
static const char *AUTO_LIBPOSTPROC_HEADERS[] = {"libpostproc/postprocess.h"};
static const char *AUTO_LIBPOSTPROC_IDENTIFIERS[] = {"pp_postprocess"};

static const char *AUTO_FFMPEG_LIBRARIES_LIBS[] = {"avcodec", "avformat", "avutil", "swscale", "swresample", "postproc"};

static const char *AUTO_GSTREAMER_LIBS[] = {"gstreamer-1.0"};
static const char *AUTO_GSTREAMER_HEADERS[] = {"gst/gst.h"};
static const char *AUTO_GSTREAMER_IDENTIFIERS[] = {"gst_init"};

static const char *AUTO_GSTREAMER_BASE_LIBS[] = {"gstreamer-base-1.0"};
static const char *AUTO_GSTREAMER_BASE_HEADERS[] = {"gst/base/gstbasesrc.h"};

static const char *AUTO_GSTREAMER_PLUGINS_BASE_LIBS[] = {"gstvideo-1.0"};
static const char *AUTO_GSTREAMER_PLUGINS_BASE_HEADERS[] = {"gst/video/video.h"};

static const char *AUTO_GSTREAMER_PLUGINS_GOOD_LIBS[] = {"gstpbutils-1.0"};
static const char *AUTO_GSTREAMER_PLUGINS_GOOD_HEADERS[] = {"gst/pbutils/pbutils.h"};

static const char *AUTO_GSTREAMER_PLUGINS_BAD_LIBS[] = {"gstcodecparsers-1.0"};
static const char *AUTO_GSTREAMER_PLUGINS_BAD_HEADERS[] = {"gst/codecparsers/gsth264parser.h"};

static const char *AUTO_GSTREAMER_PLUGINS_UGLY_LIBS[] = {"gsttag-1.0"};
static const char *AUTO_GSTREAMER_PLUGINS_UGLY_HEADERS[] = {"gst/tag/tag.h"};

static const char *AUTO_LIBMATROSKA_LIBS[] = {"matroska"};
static const char *AUTO_LIBMATROSKA_HEADERS[] = {"matroska/KaxSegment.h"};

static const char *AUTO_LIBEBML_LIBS[] = {"ebml"};
static const char *AUTO_LIBEBML_HEADERS[] = {"ebml/EbmlHead.h"};

static const char *AUTO_LIBVPX_LIBS[] = {"vpx"};
static const char *AUTO_LIBVPX_HEADERS[] = {"vpx/vpx_decoder.h"};

static const char *AUTO_DAV1D_LIBS[] = {"dav1d"};
static const char *AUTO_DAV1D_HEADERS[] = {"dav1d/dav1d.h"};

static const char *AUTO_LIBAOM_LIBS[] = {"aom"};
static const char *AUTO_LIBAOM_HEADERS[] = {"aom/aom_decoder.h"};

static const char *AUTO_X264_LIBS[] = {"x264"};
static const char *AUTO_X264_HEADERS[] = {"x264.h"};

static const char *AUTO_X265_LIBS[] = {"x265"};
static const char *AUTO_X265_HEADERS[] = {"x265.h"};

static const char *AUTO_LIBHEIF_LIBS[] = {"heif"};
static const char *AUTO_LIBHEIF_HEADERS[] = {"libheif/heif.h"};

static const char *AUTO_OPENEXR_LIBS[] = {"openexr"};
static const char *AUTO_OPENEXR_HEADERS[] = {"OpenEXR/ImfRgbaFile.h"};

static const char *AUTO_LIBTIFF_LIBS[] = {"tiff"};
static const char *AUTO_LIBTIFF_HEADERS[] = {"tiffio.h"};

static const char *AUTO_LIBWEBP_LIBS[] = {"webp"};
static const char *AUTO_LIBWEBP_HEADERS[] = {"webp/decode.h"};

static const char *AUTO_LIBOPENJP2_LIBS[] = {"openjp2"};
static const char *AUTO_LIBOPENJP2_HEADERS[] = {"openjpeg.h"};

static const char *AUTO_JASPER_LIBS[] = {"jasper"};
static const char *AUTO_JASPER_HEADERS[] = {"jasper/jasper.h"};

static const char *AUTO_LIBRAW_LIBS[] = {"raw"};
static const char *AUTO_LIBRAW_HEADERS[] = {"libraw/libraw.h"};

static const char *AUTO_LCMS2_LIBS[] = {"lcms2"};
static const char *AUTO_LCMS2_HEADERS[] = {"lcms2.h"};

static const char *AUTO_LIBEXIF_LIBS[] = {"exif"};
static const char *AUTO_LIBEXIF_HEADERS[] = {"libexif/exif-data.h"};

static const char *AUTO_EXIV2_LIBS[] = {"exiv2"};
static const char *AUTO_EXIV2_HEADERS[] = {"exiv2/exiv2.hpp"};

static const char *AUTO_LIBSPNG_LIBS[] = {"spng"};
static const char *AUTO_LIBSPNG_HEADERS[] = {"spng.h"};

static const char *AUTO_LIBGD_LIBS[] = {"gd"};
static const char *AUTO_LIBGD_HEADERS[] = {"gd.h"};

static const char *AUTO_FREETYPE_GL_LIBS[] = {"freetype-gl"};
static const char *AUTO_FREETYPE_GL_HEADERS[] = {"freetype-gl.h"};

static const char *AUTO_GLFW_LIBS[] = {"glfw"};
static const char *AUTO_GLFW_HEADERS[] = {"GLFW/glfw3.h"};
static const char *AUTO_GLFW_IDENTIFIERS[] = {"glfwInit"};

static const char *AUTO_GLEW_LIBS[] = {"GLEW"};
static const char *AUTO_GLEW_HEADERS[] = {"GL/glew.h"};
static const char *AUTO_GLEW_IDENTIFIERS[] = {"glewInit"};

static const char *AUTO_GLAD_LIBS[] = {"glad"};
static const char *AUTO_GLAD_HEADERS[] = {"glad/glad.h"};
static const char *AUTO_GLAD_IDENTIFIERS[] = {"gladLoadGLLoader"};

static const char *AUTO_MESA_LIBS[] = {"mesa"};
static const char *AUTO_MESA_HEADERS[] = {"GL/gl.h"};

static const char *AUTO_LIBEPOXY_LIBS[] = {"epoxy"};
static const char *AUTO_LIBEPOXY_HEADERS[] = {"epoxy/gl.h"};

static const char *AUTO_LIBASS_LIBS[] = {"ass"};
static const char *AUTO_LIBASS_HEADERS[] = {"ass/ass.h"};
static const char *AUTO_LIBASS_IDENTIFIERS[] = {"ass_library_init"};

static const char *AUTO_HARFBUZZ_SUBSET_LIBS[] = {"harfbuzz-subset"};
static const char *AUTO_HARFBUZZ_SUBSET_HEADERS[] = {"harfbuzz/hb-subset.h"};

static const char *AUTO_ICU_LIBS[] = {"icu"};
static const char *AUTO_ICU_HEADERS[] = {"unicode/utypes.h"};
static const char *AUTO_ICU_IDENTIFIERS[] = {"u_strToUTF8"};

static const char *AUTO_LIBIDN_LIBS[] = {"idn"};
static const char *AUTO_LIBIDN_HEADERS[] = {"idna.h"};
static const char *AUTO_LIBIDN_IDENTIFIERS[] = {"idna_to_ascii_8z"};

static const char *AUTO_LIBIDN2_LIBS[] = {"idn2"};
static const char *AUTO_LIBIDN2_HEADERS[] = {"idn2.h"};
static const char *AUTO_LIBIDN2_IDENTIFIERS[] = {"idn2_to_ascii_8z"};

static const char *AUTO_LIBPSL_LIBS[] = {"psl"};
static const char *AUTO_LIBPSL_HEADERS[] = {"libpsl.h"};
static const char *AUTO_LIBPSL_IDENTIFIERS[] = {"psl_is_public_suffix"};

static const char *AUTO_LIBTASN1_LIBS[] = {"tasn1"};
static const char *AUTO_LIBTASN1_HEADERS[] = {"libtasn1.h"};
static const char *AUTO_LIBTASN1_IDENTIFIERS[] = {"asn1_create_element"};

static const char *AUTO_LIBUNISTRING_LIBS[] = {"unistring"};
static const char *AUTO_LIBUNISTRING_HEADERS[] = {"unistr.h"};
static const char *AUTO_LIBUNISTRING_IDENTIFIERS[] = {"u8_strlen"};

static const char *AUTO_LIBICONV_LIBS[] = {"iconv"};
static const char *AUTO_LIBICONV_HEADERS[] = {"iconv.h"};
static const char *AUTO_LIBICONV_IDENTIFIERS[] = {"iconv_open"};

static const char *AUTO_LIBINTL_LIBS[] = {"intl"};
static const char *AUTO_LIBINTL_HEADERS[] = {"libintl.h"};
static const char *AUTO_LIBINTL_IDENTIFIERS[] = {"gettext"};

static const char *AUTO_LIBPCRE_LIBS[] = {"pcre"};
static const char *AUTO_LIBPCRE_HEADERS[] = {"pcre.h"};
static const char *AUTO_LIBPCRE_IDENTIFIERS[] = {"pcre_compile"};

static const char *AUTO_PCRE2_LIBS[] = {"pcre2"};
static const char *AUTO_PCRE2_HEADERS[] = {"pcre2.h"};
static const char *AUTO_PCRE2_IDENTIFIERS[] = {"pcre2_compile"};

static const char *AUTO_RE2C_LIBS[] = {"re2c"};
static const char *AUTO_RE2C_HEADERS[] = {"re2c/re2c.h"};

static const char *AUTO_ONIGURUMA_LIBS[] = {"oniguruma"};
static const char *AUTO_ONIGURUMA_HEADERS[] = {"oniguruma.h"};
static const char *AUTO_ONIGURUMA_IDENTIFIERS[] = {"onig_new"};

static const char *AUTO_LIBXMLB_LIBS[] = {"xmlb"};
static const char *AUTO_LIBXMLB_HEADERS[] = {"xmlb.h"};

static const char *AUTO_EXPAT_LIBS[] = {"expat"};
static const char *AUTO_EXPAT_HEADERS[] = {"expat.h"};
static const char *AUTO_EXPAT_IDENTIFIERS[] = {"XML_ParserCreate"};

static const char *AUTO_TINYXML2_LIBS[] = {"tinyxml2"};
static const char *AUTO_TINYXML2_HEADERS[] = {"tinyxml2.h"};
static const char *AUTO_TINYXML2_IDENTIFIERS[] = {"XMLDocument"};

static const char *AUTO_RAPIDXML_LIBS[] = {"rapidxml"};
static const char *AUTO_RAPIDXML_HEADERS[] = {"rapidxml.hpp"};

static const char *AUTO_LIBFASTJSON_LIBS[] = {"fastjson"};
static const char *AUTO_LIBFASTJSON_HEADERS[] = {"json.h"};

static const char *AUTO_YYJSON_LIBS[] = {"yyjson"};
static const char *AUTO_YYJSON_HEADERS[] = {"yyjson.h"};
static const char *AUTO_YYJSON_IDENTIFIERS[] = {"yyjson_read"};

static const char *AUTO_SIMDJSON_LIBS[] = {"simdjson"};
static const char *AUTO_SIMDJSON_HEADERS[] = {"simdjson.h"};
static const char *AUTO_SIMDJSON_IDENTIFIERS[] = {"simdjson_parse"};

static const char *AUTO_LIBCSV_LIBS[] = {"csv"};
static const char *AUTO_LIBCSV_HEADERS[] = {"csv.h"};
static const char *AUTO_LIBCSV_IDENTIFIERS[] = {"csv_init"};

static const char *AUTO_LIBXLSXWRITER_LIBS[] = {"xlsxwriter"};
static const char *AUTO_LIBXLSXWRITER_HEADERS[] = {"xlsxwriter.h"};
static const char *AUTO_LIBXLSXWRITER_IDENTIFIERS[] = {"workbook_new"};

static const char *AUTO_LIBHARU_LIBS[] = {"hpdf"};
static const char *AUTO_LIBHARU_HEADERS[] = {"hpdf.h"};
static const char *AUTO_LIBHARU_IDENTIFIERS[] = {"HPDF_New"};

static const char *AUTO_MUPDF_LIBS[] = {"mupdf"};
static const char *AUTO_MUPDF_HEADERS[] = {"mupdf/fitz.h"};
static const char *AUTO_MUPDF_IDENTIFIERS[] = {"fz_new_context"};

static const char *AUTO_POPPLER_LIBS[] = {"poppler"};
static const char *AUTO_POPPLER_HEADERS[] = {"poppler/cpp/poppler-document.h"};
static const char *AUTO_POPPLER_IDENTIFIERS[] = {"poppler_document_new_from_file"};

static const char *AUTO_QPDF_LIBS[] = {"qpdf"};
static const char *AUTO_QPDF_HEADERS[] = {"qpdf/QPDF.hh"};
static const char *AUTO_QPDF_IDENTIFIERS[] = {"QPDF"};

static const char *AUTO_LIBARCHIVE_C_LIBS[] = {"archive"};
static const char *AUTO_LIBARCHIVE_C_HEADERS[] = {"archive.h"};

static const char *AUTO_MINIZ_LIBS[] = {"miniz"};
static const char *AUTO_MINIZ_HEADERS[] = {"miniz.h"};
static const char *AUTO_MINIZ_IDENTIFIERS[] = {"mz_uncompress"};

static const char *AUTO_LZ4_LIBS[] = {"lz4"};
static const char *AUTO_LZ4_HEADERS[] = {"lz4.h"};
static const char *AUTO_LZ4_IDENTIFIERS[] = {"LZ4_compress_default"};

static const char *AUTO_ZSTD_LIBS[] = {"zstd"};
static const char *AUTO_ZSTD_HEADERS[] = {"zstd.h"};
static const char *AUTO_ZSTD_IDENTIFIERS[] = {"ZSTD_compress"};

static const char *AUTO_BROTLI_LIBS[] = {"brotli"};
static const char *AUTO_BROTLI_HEADERS[] = {"brotli/decode.h"};
static const char *AUTO_BROTLI_IDENTIFIERS[] = {"BrotliDecoderCreateInstance"};

static const char *AUTO_LIBDEFLATE_LIBS[] = {"deflate"};
static const char *AUTO_LIBDEFLATE_HEADERS[] = {"libdeflate.h"};
static const char *AUTO_LIBDEFLATE_IDENTIFIERS[] = {"libdeflate_alloc_compressor"};

static const char *AUTO_SNAPPY_LIBS[] = {"snappy"};
static const char *AUTO_SNAPPY_HEADERS[] = {"snappy-c.h"};
static const char *AUTO_SNAPPY_IDENTIFIERS[] = {"snappy_compress"};

static const char *AUTO_ISA_L_LIBS[] = {"isal"};
static const char *AUTO_ISA_L_HEADERS[] = {"isa-l.h"};

static const char *AUTO_TBBMALLOC_LIBS[] = {"tbbmalloc"};
static const char *AUTO_TBBMALLOC_HEADERS[] = {"tbbmalloc_proxy.h"};

static const char *AUTO_JEMALLOC_LIBS[] = {"jemalloc"};
static const char *AUTO_JEMALLOC_HEADERS[] = {"jemalloc/jemalloc.h"};
static const char *AUTO_JEMALLOC_IDENTIFIERS[] = {"mallocx"};

static const char *AUTO_TCMALLOC_LIBS[] = {"tcmalloc"};
static const char *AUTO_TCMALLOC_HEADERS[] = {"gperftools/tcmalloc.h"};

static const char *AUTO_MIMALLOC_LIBS[] = {"mimalloc"};
static const char *AUTO_MIMALLOC_HEADERS[] = {"mimalloc.h"};
static const char *AUTO_MIMALLOC_IDENTIFIERS[] = {"mi_malloc"};

static const char *AUTO_BOEHM_GC_LIBS[] = {"gc"};
static const char *AUTO_BOEHM_GC_HEADERS[] = {"gc.h"};
static const char *AUTO_BOEHM_GC_IDENTIFIERS[] = {"GC_malloc"};

static const char *AUTO_LIBUVC_LIBS[] = {"uvc"};
static const char *AUTO_LIBUVC_HEADERS[] = {"libuvc/libuvc.h"};
static const char *AUTO_LIBUVC_IDENTIFIERS[] = {"uvc_init"};

static const char *AUTO_LIBCAMERA_LIBS[] = {"camera"};
static const char *AUTO_LIBCAMERA_HEADERS[] = {"libcamera/libcamera.h"};

static const char *AUTO_LIBFREENECT_LIBS[] = {"freenect"};
static const char *AUTO_LIBFREENECT_HEADERS[] = {"libfreenect.h"};
static const char *AUTO_LIBFREENECT_IDENTIFIERS[] = {"freenect_init"};

static const char *AUTO_LIBREALSENSE_LIBS[] = {"realsense2"};
static const char *AUTO_LIBREALSENSE_HEADERS[] = {"librealsense2/rs.hpp"};

static const char *AUTO_LIBHIDAPI_LIBS[] = {"hidapi"};
static const char *AUTO_LIBHIDAPI_HEADERS[] = {"hidapi/hidapi.h"};
static const char *AUTO_LIBHIDAPI_IDENTIFIERS[] = {"hid_init"};

static const char *AUTO_LIBIIO_LIBS[] = {"iio"};
static const char *AUTO_LIBIIO_HEADERS[] = {"iio.h"};
static const char *AUTO_LIBIIO_IDENTIFIERS[] = {"iio_create_context"};

static const char *AUTO_LIBFTD2XX_LIBS[] = {"ftd2xx"};
static const char *AUTO_LIBFTD2XX_HEADERS[] = {"ftd2xx.h"};

static const char *AUTO_LIBGPHOTO2_LIBS[] = {"gphoto2"};
static const char *AUTO_LIBGPHOTO2_HEADERS[] = {"gphoto2/gphoto2-camera.h"};
static const char *AUTO_LIBGPHOTO2_IDENTIFIERS[] = {"gp_camera_new"};

static const char *AUTO_LIBMTP_LIBS[] = {"mtp"};
static const char *AUTO_LIBMTP_HEADERS[] = {"libmtp.h"};
static const char *AUTO_LIBMTP_IDENTIFIERS[] = {"LIBMTP_Init"};

static const char *AUTO_LIBBLURAY_LIBS[] = {"bluray"};
static const char *AUTO_LIBBLURAY_HEADERS[] = {"libbluray/bluray.h"};
static const char *AUTO_LIBBLURAY_IDENTIFIERS[] = {"bd_open"};

static const char *AUTO_LIBDVDNAV_LIBS[] = {"dvdnav"};
static const char *AUTO_LIBDVDNAV_HEADERS[] = {"dvdnav/dvdnav.h"};
static const char *AUTO_LIBDVDNAV_IDENTIFIERS[] = {"dvdnav_open"};

static const char *AUTO_LIBDVDREAD_LIBS[] = {"dvdread"};
static const char *AUTO_LIBDVDREAD_HEADERS[] = {"dvdread/dvd_reader.h"};
static const char *AUTO_LIBDVDREAD_IDENTIFIERS[] = {"DVDOpen"};

static const AutoLibRule AUTO_LIB_RULES[] = {
    {M_LIBS, MAZEN_ARRAY_LEN(M_LIBS), M_HEADERS, MAZEN_ARRAY_LEN(M_HEADERS), M_IDENTIFIERS,
     MAZEN_ARRAY_LEN(M_IDENTIFIERS), NULL, 0},
    {PTHREAD_LIBS, MAZEN_ARRAY_LEN(PTHREAD_LIBS), PTHREAD_HEADERS, MAZEN_ARRAY_LEN(PTHREAD_HEADERS),
     PTHREAD_IDENTIFIERS, MAZEN_ARRAY_LEN(PTHREAD_IDENTIFIERS), PTHREAD_PREFIXES, MAZEN_ARRAY_LEN(PTHREAD_PREFIXES)},
    {DL_LIBS, MAZEN_ARRAY_LEN(DL_LIBS), DL_HEADERS, MAZEN_ARRAY_LEN(DL_HEADERS), DL_IDENTIFIERS,
     MAZEN_ARRAY_LEN(DL_IDENTIFIERS), NULL, 0},
    {RT_LIBS, MAZEN_ARRAY_LEN(RT_LIBS), RT_HEADERS, MAZEN_ARRAY_LEN(RT_HEADERS), RT_IDENTIFIERS,
     MAZEN_ARRAY_LEN(RT_IDENTIFIERS), RT_PREFIXES, MAZEN_ARRAY_LEN(RT_PREFIXES)},
    {CURL_LIBS, MAZEN_ARRAY_LEN(CURL_LIBS), CURL_HEADERS, MAZEN_ARRAY_LEN(CURL_HEADERS), CURL_IDENTIFIERS,
     MAZEN_ARRAY_LEN(CURL_IDENTIFIERS), CURL_PREFIXES, MAZEN_ARRAY_LEN(CURL_PREFIXES)},
    {OPENSSL_LIBS, MAZEN_ARRAY_LEN(OPENSSL_LIBS), OPENSSL_HEADERS, MAZEN_ARRAY_LEN(OPENSSL_HEADERS),
     OPENSSL_IDENTIFIERS, MAZEN_ARRAY_LEN(OPENSSL_IDENTIFIERS), OPENSSL_PREFIXES,
     MAZEN_ARRAY_LEN(OPENSSL_PREFIXES)},
    {ZLIB_LIBS, MAZEN_ARRAY_LEN(ZLIB_LIBS), ZLIB_HEADERS, MAZEN_ARRAY_LEN(ZLIB_HEADERS), ZLIB_IDENTIFIERS,
     MAZEN_ARRAY_LEN(ZLIB_IDENTIFIERS), ZLIB_PREFIXES, MAZEN_ARRAY_LEN(ZLIB_PREFIXES)},
    {PNG_LIBS, MAZEN_ARRAY_LEN(PNG_LIBS), PNG_HEADERS, MAZEN_ARRAY_LEN(PNG_HEADERS), PNG_IDENTIFIERS,
     MAZEN_ARRAY_LEN(PNG_IDENTIFIERS), PNG_PREFIXES, MAZEN_ARRAY_LEN(PNG_PREFIXES)},
    {JPEG_LIBS, MAZEN_ARRAY_LEN(JPEG_LIBS), JPEG_HEADERS, MAZEN_ARRAY_LEN(JPEG_HEADERS), JPEG_IDENTIFIERS,
     MAZEN_ARRAY_LEN(JPEG_IDENTIFIERS), JPEG_PREFIXES, MAZEN_ARRAY_LEN(JPEG_PREFIXES)},
    {XML2_LIBS, MAZEN_ARRAY_LEN(XML2_LIBS), XML2_HEADERS, MAZEN_ARRAY_LEN(XML2_HEADERS), XML2_IDENTIFIERS,
     MAZEN_ARRAY_LEN(XML2_IDENTIFIERS), XML2_PREFIXES, MAZEN_ARRAY_LEN(XML2_PREFIXES)},
    {YAML_LIBS, MAZEN_ARRAY_LEN(YAML_LIBS), YAML_HEADERS, MAZEN_ARRAY_LEN(YAML_HEADERS), YAML_IDENTIFIERS,
     MAZEN_ARRAY_LEN(YAML_IDENTIFIERS), YAML_PREFIXES, MAZEN_ARRAY_LEN(YAML_PREFIXES)},
    {CJSON_LIBS, MAZEN_ARRAY_LEN(CJSON_LIBS), CJSON_HEADERS, MAZEN_ARRAY_LEN(CJSON_HEADERS), CJSON_IDENTIFIERS,
     MAZEN_ARRAY_LEN(CJSON_IDENTIFIERS), CJSON_PREFIXES, MAZEN_ARRAY_LEN(CJSON_PREFIXES)},
    {JANSSON_LIBS, MAZEN_ARRAY_LEN(JANSSON_LIBS), JANSSON_HEADERS, MAZEN_ARRAY_LEN(JANSSON_HEADERS),
     JANSSON_IDENTIFIERS, MAZEN_ARRAY_LEN(JANSSON_IDENTIFIERS), NULL, 0},
    {SQLITE_LIBS, MAZEN_ARRAY_LEN(SQLITE_LIBS), SQLITE_HEADERS, MAZEN_ARRAY_LEN(SQLITE_HEADERS),
     SQLITE_IDENTIFIERS, MAZEN_ARRAY_LEN(SQLITE_IDENTIFIERS), SQLITE_PREFIXES, MAZEN_ARRAY_LEN(SQLITE_PREFIXES)},
    {GIT2_LIBS, MAZEN_ARRAY_LEN(GIT2_LIBS), GIT2_HEADERS, MAZEN_ARRAY_LEN(GIT2_HEADERS), GIT2_IDENTIFIERS,
     MAZEN_ARRAY_LEN(GIT2_IDENTIFIERS), GIT2_PREFIXES, MAZEN_ARRAY_LEN(GIT2_PREFIXES)},
    {UV_LIBS, MAZEN_ARRAY_LEN(UV_LIBS), UV_HEADERS, MAZEN_ARRAY_LEN(UV_HEADERS), UV_IDENTIFIERS,
     MAZEN_ARRAY_LEN(UV_IDENTIFIERS), UV_PREFIXES, MAZEN_ARRAY_LEN(UV_PREFIXES)},
    {GLIB_LIBS, MAZEN_ARRAY_LEN(GLIB_LIBS), GLIB_HEADERS, MAZEN_ARRAY_LEN(GLIB_HEADERS), GLIB_IDENTIFIERS,
     MAZEN_ARRAY_LEN(GLIB_IDENTIFIERS), GLIB_PREFIXES, MAZEN_ARRAY_LEN(GLIB_PREFIXES)},
    {GOBJECT_LIBS, MAZEN_ARRAY_LEN(GOBJECT_LIBS), GOBJECT_HEADERS, MAZEN_ARRAY_LEN(GOBJECT_HEADERS),
     GOBJECT_IDENTIFIERS, MAZEN_ARRAY_LEN(GOBJECT_IDENTIFIERS), GOBJECT_PREFIXES,
     MAZEN_ARRAY_LEN(GOBJECT_PREFIXES)},
    {NCURSES_LIBS, MAZEN_ARRAY_LEN(NCURSES_LIBS), NCURSES_HEADERS, MAZEN_ARRAY_LEN(NCURSES_HEADERS),
     NCURSES_IDENTIFIERS, MAZEN_ARRAY_LEN(NCURSES_IDENTIFIERS), NULL, 0},
    {READLINE_LIBS, MAZEN_ARRAY_LEN(READLINE_LIBS), READLINE_HEADERS, MAZEN_ARRAY_LEN(READLINE_HEADERS),
     READLINE_IDENTIFIERS, MAZEN_ARRAY_LEN(READLINE_IDENTIFIERS), READLINE_PREFIXES,
     MAZEN_ARRAY_LEN(READLINE_PREFIXES)},
    {SDL2_LIBS, MAZEN_ARRAY_LEN(SDL2_LIBS), SDL2_HEADERS, MAZEN_ARRAY_LEN(SDL2_HEADERS), SDL2_IDENTIFIERS,
     MAZEN_ARRAY_LEN(SDL2_IDENTIFIERS), SDL2_PREFIXES, MAZEN_ARRAY_LEN(SDL2_PREFIXES)},
    {ALLEGRO_LIBS, MAZEN_ARRAY_LEN(ALLEGRO_LIBS), ALLEGRO_HEADERS, MAZEN_ARRAY_LEN(ALLEGRO_HEADERS),
     ALLEGRO_IDENTIFIERS, MAZEN_ARRAY_LEN(ALLEGRO_IDENTIFIERS), ALLEGRO_PREFIXES,
     MAZEN_ARRAY_LEN(ALLEGRO_PREFIXES)},
    {RAYLIB_LIBS, MAZEN_ARRAY_LEN(RAYLIB_LIBS), RAYLIB_HEADERS, MAZEN_ARRAY_LEN(RAYLIB_HEADERS),
     RAYLIB_IDENTIFIERS, MAZEN_ARRAY_LEN(RAYLIB_IDENTIFIERS), NULL, 0},
    {OPENAL_LIBS, MAZEN_ARRAY_LEN(OPENAL_LIBS), OPENAL_HEADERS, MAZEN_ARRAY_LEN(OPENAL_HEADERS),
     OPENAL_IDENTIFIERS, MAZEN_ARRAY_LEN(OPENAL_IDENTIFIERS), OPENAL_PREFIXES, MAZEN_ARRAY_LEN(OPENAL_PREFIXES)},
    {PORTAUDIO_LIBS, MAZEN_ARRAY_LEN(PORTAUDIO_LIBS), PORTAUDIO_HEADERS, MAZEN_ARRAY_LEN(PORTAUDIO_HEADERS),
     PORTAUDIO_IDENTIFIERS, MAZEN_ARRAY_LEN(PORTAUDIO_IDENTIFIERS), PORTAUDIO_PREFIXES,
     MAZEN_ARRAY_LEN(PORTAUDIO_PREFIXES)},
    {PULSE_LIBS, MAZEN_ARRAY_LEN(PULSE_LIBS), PULSE_HEADERS, MAZEN_ARRAY_LEN(PULSE_HEADERS), PULSE_IDENTIFIERS,
     MAZEN_ARRAY_LEN(PULSE_IDENTIFIERS), PULSE_PREFIXES, MAZEN_ARRAY_LEN(PULSE_PREFIXES)},
    {PULSE_SIMPLE_LIBS, MAZEN_ARRAY_LEN(PULSE_SIMPLE_LIBS), PULSE_SIMPLE_HEADERS,
     MAZEN_ARRAY_LEN(PULSE_SIMPLE_HEADERS), PULSE_SIMPLE_IDENTIFIERS, MAZEN_ARRAY_LEN(PULSE_SIMPLE_IDENTIFIERS),
     PULSE_SIMPLE_PREFIXES, MAZEN_ARRAY_LEN(PULSE_SIMPLE_PREFIXES)},
    {PIPEWIRE_LIBS, MAZEN_ARRAY_LEN(PIPEWIRE_LIBS), PIPEWIRE_HEADERS, MAZEN_ARRAY_LEN(PIPEWIRE_HEADERS),
     PIPEWIRE_IDENTIFIERS, MAZEN_ARRAY_LEN(PIPEWIRE_IDENTIFIERS), PIPEWIRE_PREFIXES,
     MAZEN_ARRAY_LEN(PIPEWIRE_PREFIXES)},
    {EVENT_LIBS, MAZEN_ARRAY_LEN(EVENT_LIBS), EVENT_HEADERS, MAZEN_ARRAY_LEN(EVENT_HEADERS), EVENT_IDENTIFIERS,
     MAZEN_ARRAY_LEN(EVENT_IDENTIFIERS), EVENT_PREFIXES, MAZEN_ARRAY_LEN(EVENT_PREFIXES)},
    {EV_LIBS, MAZEN_ARRAY_LEN(EV_LIBS), EV_HEADERS, MAZEN_ARRAY_LEN(EV_HEADERS), EV_IDENTIFIERS,
     MAZEN_ARRAY_LEN(EV_IDENTIFIERS), EV_PREFIXES, MAZEN_ARRAY_LEN(EV_PREFIXES)},
    {FFI_LIBS, MAZEN_ARRAY_LEN(FFI_LIBS), FFI_HEADERS, MAZEN_ARRAY_LEN(FFI_HEADERS), FFI_IDENTIFIERS,
     MAZEN_ARRAY_LEN(FFI_IDENTIFIERS), FFI_PREFIXES, MAZEN_ARRAY_LEN(FFI_PREFIXES)},
    {ARCHIVE_LIBS, MAZEN_ARRAY_LEN(ARCHIVE_LIBS), ARCHIVE_HEADERS, MAZEN_ARRAY_LEN(ARCHIVE_HEADERS),
     ARCHIVE_IDENTIFIERS, MAZEN_ARRAY_LEN(ARCHIVE_IDENTIFIERS), ARCHIVE_PREFIXES, MAZEN_ARRAY_LEN(ARCHIVE_PREFIXES)},
    {ZIP_LIBS, MAZEN_ARRAY_LEN(ZIP_LIBS), ZIP_HEADERS, MAZEN_ARRAY_LEN(ZIP_HEADERS), ZIP_IDENTIFIERS,
     MAZEN_ARRAY_LEN(ZIP_IDENTIFIERS), ZIP_PREFIXES, MAZEN_ARRAY_LEN(ZIP_PREFIXES)},
    {BZ2_LIBS, MAZEN_ARRAY_LEN(BZ2_LIBS), BZ2_HEADERS, MAZEN_ARRAY_LEN(BZ2_HEADERS), BZ2_IDENTIFIERS,
     MAZEN_ARRAY_LEN(BZ2_IDENTIFIERS), BZ2_PREFIXES, MAZEN_ARRAY_LEN(BZ2_PREFIXES)},
    {LZMA_LIBS, MAZEN_ARRAY_LEN(LZMA_LIBS), LZMA_HEADERS, MAZEN_ARRAY_LEN(LZMA_HEADERS), LZMA_IDENTIFIERS,
     MAZEN_ARRAY_LEN(LZMA_IDENTIFIERS), LZMA_PREFIXES, MAZEN_ARRAY_LEN(LZMA_PREFIXES)},
    {PROTOBUFC_LIBS, MAZEN_ARRAY_LEN(PROTOBUFC_LIBS), PROTOBUFC_HEADERS, MAZEN_ARRAY_LEN(PROTOBUFC_HEADERS),
     PROTOBUFC_IDENTIFIERS, MAZEN_ARRAY_LEN(PROTOBUFC_IDENTIFIERS), PROTOBUFC_PREFIXES,
     MAZEN_ARRAY_LEN(PROTOBUFC_PREFIXES)},
    {NANOMSG_LIBS, MAZEN_ARRAY_LEN(NANOMSG_LIBS), NANOMSG_HEADERS, MAZEN_ARRAY_LEN(NANOMSG_HEADERS),
     NANOMSG_IDENTIFIERS, MAZEN_ARRAY_LEN(NANOMSG_IDENTIFIERS), NANOMSG_PREFIXES,
     MAZEN_ARRAY_LEN(NANOMSG_PREFIXES)},
    {NNG_LIBS, MAZEN_ARRAY_LEN(NNG_LIBS), NNG_HEADERS, MAZEN_ARRAY_LEN(NNG_HEADERS), NNG_IDENTIFIERS,
     MAZEN_ARRAY_LEN(NNG_IDENTIFIERS), NNG_PREFIXES, MAZEN_ARRAY_LEN(NNG_PREFIXES)},
    {ZMQ_LIBS, MAZEN_ARRAY_LEN(ZMQ_LIBS), ZMQ_HEADERS, MAZEN_ARRAY_LEN(ZMQ_HEADERS), ZMQ_IDENTIFIERS,
     MAZEN_ARRAY_LEN(ZMQ_IDENTIFIERS), ZMQ_PREFIXES, MAZEN_ARRAY_LEN(ZMQ_PREFIXES)},
    {HIREDIS_LIBS, MAZEN_ARRAY_LEN(HIREDIS_LIBS), HIREDIS_HEADERS, MAZEN_ARRAY_LEN(HIREDIS_HEADERS),
     HIREDIS_IDENTIFIERS, MAZEN_ARRAY_LEN(HIREDIS_IDENTIFIERS), HIREDIS_PREFIXES,
     MAZEN_ARRAY_LEN(HIREDIS_PREFIXES)},
    {PQ_LIBS, MAZEN_ARRAY_LEN(PQ_LIBS), PQ_HEADERS, MAZEN_ARRAY_LEN(PQ_HEADERS), PQ_IDENTIFIERS,
     MAZEN_ARRAY_LEN(PQ_IDENTIFIERS), PQ_PREFIXES, MAZEN_ARRAY_LEN(PQ_PREFIXES)},
    {MYSQL_LIBS, MAZEN_ARRAY_LEN(MYSQL_LIBS), MYSQL_HEADERS, MAZEN_ARRAY_LEN(MYSQL_HEADERS), MYSQL_IDENTIFIERS,
     MAZEN_ARRAY_LEN(MYSQL_IDENTIFIERS), MYSQL_PREFIXES, MAZEN_ARRAY_LEN(MYSQL_PREFIXES)},
    {MONGOC_LIBS, MAZEN_ARRAY_LEN(MONGOC_LIBS), MONGOC_HEADERS, MAZEN_ARRAY_LEN(MONGOC_HEADERS),
     MONGOC_IDENTIFIERS, MAZEN_ARRAY_LEN(MONGOC_IDENTIFIERS), MONGOC_PREFIXES, MAZEN_ARRAY_LEN(MONGOC_PREFIXES)},
    {CAIRO_LIBS, MAZEN_ARRAY_LEN(CAIRO_LIBS), CAIRO_HEADERS, MAZEN_ARRAY_LEN(CAIRO_HEADERS), CAIRO_IDENTIFIERS,
     MAZEN_ARRAY_LEN(CAIRO_IDENTIFIERS), CAIRO_PREFIXES, MAZEN_ARRAY_LEN(CAIRO_PREFIXES)},
    {PANGO_LIBS, MAZEN_ARRAY_LEN(PANGO_LIBS), PANGO_HEADERS, MAZEN_ARRAY_LEN(PANGO_HEADERS), PANGO_IDENTIFIERS,
     MAZEN_ARRAY_LEN(PANGO_IDENTIFIERS), PANGO_PREFIXES, MAZEN_ARRAY_LEN(PANGO_PREFIXES)},
    {FREETYPE_LIBS, MAZEN_ARRAY_LEN(FREETYPE_LIBS), FREETYPE_HEADERS, MAZEN_ARRAY_LEN(FREETYPE_HEADERS),
     FREETYPE_IDENTIFIERS, MAZEN_ARRAY_LEN(FREETYPE_IDENTIFIERS), FREETYPE_PREFIXES,
     MAZEN_ARRAY_LEN(FREETYPE_PREFIXES)},
    {HARFBUZZ_LIBS, MAZEN_ARRAY_LEN(HARFBUZZ_LIBS), HARFBUZZ_HEADERS, MAZEN_ARRAY_LEN(HARFBUZZ_HEADERS),
     HARFBUZZ_IDENTIFIERS, MAZEN_ARRAY_LEN(HARFBUZZ_IDENTIFIERS), HARFBUZZ_PREFIXES,
     MAZEN_ARRAY_LEN(HARFBUZZ_PREFIXES)},
    {DRM_LIBS, MAZEN_ARRAY_LEN(DRM_LIBS), DRM_HEADERS, MAZEN_ARRAY_LEN(DRM_HEADERS), DRM_IDENTIFIERS,
     MAZEN_ARRAY_LEN(DRM_IDENTIFIERS), DRM_PREFIXES, MAZEN_ARRAY_LEN(DRM_PREFIXES)},
    {VULKAN_LIBS, MAZEN_ARRAY_LEN(VULKAN_LIBS), VULKAN_HEADERS, MAZEN_ARRAY_LEN(VULKAN_HEADERS),
     VULKAN_IDENTIFIERS, MAZEN_ARRAY_LEN(VULKAN_IDENTIFIERS), VULKAN_PREFIXES, MAZEN_ARRAY_LEN(VULKAN_PREFIXES)},
    {GLEW_LIBS, MAZEN_ARRAY_LEN(GLEW_LIBS), GLEW_HEADERS, MAZEN_ARRAY_LEN(GLEW_HEADERS), GLEW_IDENTIFIERS,
     MAZEN_ARRAY_LEN(GLEW_IDENTIFIERS), GLEW_PREFIXES, MAZEN_ARRAY_LEN(GLEW_PREFIXES)},
    {LIBUSB_LIBS, MAZEN_ARRAY_LEN(LIBUSB_LIBS), LIBUSB_HEADERS, MAZEN_ARRAY_LEN(LIBUSB_HEADERS),
     LIBUSB_IDENTIFIERS, MAZEN_ARRAY_LEN(LIBUSB_IDENTIFIERS), LIBUSB_PREFIXES, MAZEN_ARRAY_LEN(LIBUSB_PREFIXES)},
    {PCAP_LIBS, MAZEN_ARRAY_LEN(PCAP_LIBS), PCAP_HEADERS, MAZEN_ARRAY_LEN(PCAP_HEADERS), PCAP_IDENTIFIERS,
     MAZEN_ARRAY_LEN(PCAP_IDENTIFIERS), PCAP_PREFIXES, MAZEN_ARRAY_LEN(PCAP_PREFIXES)},
    {UUID_LIBS, MAZEN_ARRAY_LEN(UUID_LIBS), UUID_HEADERS, MAZEN_ARRAY_LEN(UUID_HEADERS), UUID_IDENTIFIERS,
     MAZEN_ARRAY_LEN(UUID_IDENTIFIERS), UUID_PREFIXES, MAZEN_ARRAY_LEN(UUID_PREFIXES)},
    {BLKID_LIBS, MAZEN_ARRAY_LEN(BLKID_LIBS), BLKID_HEADERS, MAZEN_ARRAY_LEN(BLKID_HEADERS), BLKID_IDENTIFIERS,
     MAZEN_ARRAY_LEN(BLKID_IDENTIFIERS), BLKID_PREFIXES, MAZEN_ARRAY_LEN(BLKID_PREFIXES)},
    {MOUNT_LIBS, MAZEN_ARRAY_LEN(MOUNT_LIBS), MOUNT_HEADERS, MAZEN_ARRAY_LEN(MOUNT_HEADERS), MOUNT_IDENTIFIERS,
     MAZEN_ARRAY_LEN(MOUNT_IDENTIFIERS), MOUNT_PREFIXES, MAZEN_ARRAY_LEN(MOUNT_PREFIXES)},
    {SECCOMP_LIBS, MAZEN_ARRAY_LEN(SECCOMP_LIBS), SECCOMP_HEADERS, MAZEN_ARRAY_LEN(SECCOMP_HEADERS),
     SECCOMP_IDENTIFIERS, MAZEN_ARRAY_LEN(SECCOMP_IDENTIFIERS), SECCOMP_PREFIXES,
     MAZEN_ARRAY_LEN(SECCOMP_PREFIXES)},
    {CAP_LIBS, MAZEN_ARRAY_LEN(CAP_LIBS), CAP_HEADERS, MAZEN_ARRAY_LEN(CAP_HEADERS), CAP_IDENTIFIERS,
     MAZEN_ARRAY_LEN(CAP_IDENTIFIERS), CAP_PREFIXES, MAZEN_ARRAY_LEN(CAP_PREFIXES)},
    {SYSTEMD_LIBS, MAZEN_ARRAY_LEN(SYSTEMD_LIBS), SYSTEMD_HEADERS, MAZEN_ARRAY_LEN(SYSTEMD_HEADERS),
     SYSTEMD_IDENTIFIERS, MAZEN_ARRAY_LEN(SYSTEMD_IDENTIFIERS), SYSTEMD_PREFIXES,
     MAZEN_ARRAY_LEN(SYSTEMD_PREFIXES)},
    {UDEV_LIBS, MAZEN_ARRAY_LEN(UDEV_LIBS), UDEV_HEADERS, MAZEN_ARRAY_LEN(UDEV_HEADERS), UDEV_IDENTIFIERS,
     MAZEN_ARRAY_LEN(UDEV_IDENTIFIERS), UDEV_PREFIXES, MAZEN_ARRAY_LEN(UDEV_PREFIXES)},
    {PCI_LIBS, MAZEN_ARRAY_LEN(PCI_LIBS), PCI_HEADERS, MAZEN_ARRAY_LEN(PCI_HEADERS), PCI_IDENTIFIERS,
     MAZEN_ARRAY_LEN(PCI_IDENTIFIERS), PCI_PREFIXES, MAZEN_ARRAY_LEN(PCI_PREFIXES)},
    {LIBNL_LIBS, MAZEN_ARRAY_LEN(LIBNL_LIBS), LIBNL_HEADERS, MAZEN_ARRAY_LEN(LIBNL_HEADERS), LIBNL_IDENTIFIERS,
     MAZEN_ARRAY_LEN(LIBNL_IDENTIFIERS), LIBNL_PREFIXES, MAZEN_ARRAY_LEN(LIBNL_PREFIXES)},
    {LIBNET_LIBS, MAZEN_ARRAY_LEN(LIBNET_LIBS), LIBNET_HEADERS, MAZEN_ARRAY_LEN(LIBNET_HEADERS),
     LIBNET_IDENTIFIERS, MAZEN_ARRAY_LEN(LIBNET_IDENTIFIERS), LIBNET_PREFIXES, MAZEN_ARRAY_LEN(LIBNET_PREFIXES)},
    {SSH_LIBS, MAZEN_ARRAY_LEN(SSH_LIBS), SSH_HEADERS, MAZEN_ARRAY_LEN(SSH_HEADERS), SSH_IDENTIFIERS,
     MAZEN_ARRAY_LEN(SSH_IDENTIFIERS), SSH_PREFIXES, MAZEN_ARRAY_LEN(SSH_PREFIXES)},
    {SSH2_LIBS, MAZEN_ARRAY_LEN(SSH2_LIBS), SSH2_HEADERS, MAZEN_ARRAY_LEN(SSH2_HEADERS), SSH2_IDENTIFIERS,
     MAZEN_ARRAY_LEN(SSH2_IDENTIFIERS), SSH2_PREFIXES, MAZEN_ARRAY_LEN(SSH2_PREFIXES)},
    {GCRYPT_LIBS, MAZEN_ARRAY_LEN(GCRYPT_LIBS), GCRYPT_HEADERS, MAZEN_ARRAY_LEN(GCRYPT_HEADERS),
     GCRYPT_IDENTIFIERS, MAZEN_ARRAY_LEN(GCRYPT_IDENTIFIERS), GCRYPT_PREFIXES,
     MAZEN_ARRAY_LEN(GCRYPT_PREFIXES)},
    {TASN1_LIBS, MAZEN_ARRAY_LEN(TASN1_LIBS), TASN1_HEADERS, MAZEN_ARRAY_LEN(TASN1_HEADERS), TASN1_IDENTIFIERS,
     MAZEN_ARRAY_LEN(TASN1_IDENTIFIERS), TASN1_PREFIXES, MAZEN_ARRAY_LEN(TASN1_PREFIXES)},
    {NETTLE_LIBS, MAZEN_ARRAY_LEN(NETTLE_LIBS), NETTLE_HEADERS, MAZEN_ARRAY_LEN(NETTLE_HEADERS),
     NETTLE_IDENTIFIERS, MAZEN_ARRAY_LEN(NETTLE_IDENTIFIERS), NETTLE_PREFIXES, MAZEN_ARRAY_LEN(NETTLE_PREFIXES)},
    {GNUTLS_LIBS, MAZEN_ARRAY_LEN(GNUTLS_LIBS), GNUTLS_HEADERS, MAZEN_ARRAY_LEN(GNUTLS_HEADERS),
     GNUTLS_IDENTIFIERS, MAZEN_ARRAY_LEN(GNUTLS_IDENTIFIERS), GNUTLS_PREFIXES, MAZEN_ARRAY_LEN(GNUTLS_PREFIXES)},
    {MBEDTLS_LIBS, MAZEN_ARRAY_LEN(MBEDTLS_LIBS), MBEDTLS_HEADERS, MAZEN_ARRAY_LEN(MBEDTLS_HEADERS),
     MBEDTLS_IDENTIFIERS, MAZEN_ARRAY_LEN(MBEDTLS_IDENTIFIERS), MBEDTLS_PREFIXES,
     MAZEN_ARRAY_LEN(MBEDTLS_PREFIXES)},
    {WOLFSSL_LIBS, MAZEN_ARRAY_LEN(WOLFSSL_LIBS), WOLFSSL_HEADERS, MAZEN_ARRAY_LEN(WOLFSSL_HEADERS),
     WOLFSSL_IDENTIFIERS, MAZEN_ARRAY_LEN(WOLFSSL_IDENTIFIERS), WOLFSSL_PREFIXES,
     MAZEN_ARRAY_LEN(WOLFSSL_PREFIXES)},
    {SODIUM_LIBS, MAZEN_ARRAY_LEN(SODIUM_LIBS), SODIUM_HEADERS, MAZEN_ARRAY_LEN(SODIUM_HEADERS),
     SODIUM_IDENTIFIERS, MAZEN_ARRAY_LEN(SODIUM_IDENTIFIERS), NULL, 0},
    {ARGON2_LIBS, MAZEN_ARRAY_LEN(ARGON2_LIBS), ARGON2_HEADERS, MAZEN_ARRAY_LEN(ARGON2_HEADERS),
     ARGON2_IDENTIFIERS, MAZEN_ARRAY_LEN(ARGON2_IDENTIFIERS), ARGON2_PREFIXES, MAZEN_ARRAY_LEN(ARGON2_PREFIXES)},
    {BCRYPT_LIBS, MAZEN_ARRAY_LEN(BCRYPT_LIBS), BCRYPT_HEADERS, MAZEN_ARRAY_LEN(BCRYPT_HEADERS),
     BCRYPT_IDENTIFIERS, MAZEN_ARRAY_LEN(BCRYPT_IDENTIFIERS), BCRYPT_PREFIXES, MAZEN_ARRAY_LEN(BCRYPT_PREFIXES)},
    {EDIT_LIBS, MAZEN_ARRAY_LEN(EDIT_LIBS), EDIT_HEADERS, MAZEN_ARRAY_LEN(EDIT_HEADERS), EDIT_IDENTIFIERS,
     MAZEN_ARRAY_LEN(EDIT_IDENTIFIERS), NULL, 0},
    {CONFIG_LIBS, MAZEN_ARRAY_LEN(CONFIG_LIBS), CONFIG_HEADERS, MAZEN_ARRAY_LEN(CONFIG_HEADERS),
     CONFIG_IDENTIFIERS, MAZEN_ARRAY_LEN(CONFIG_IDENTIFIERS), CONFIG_PREFIXES, MAZEN_ARRAY_LEN(CONFIG_PREFIXES)},
    {INIPARSER_LIBS, MAZEN_ARRAY_LEN(INIPARSER_LIBS), INIPARSER_HEADERS, MAZEN_ARRAY_LEN(INIPARSER_HEADERS),
     INIPARSER_IDENTIFIERS, MAZEN_ARRAY_LEN(INIPARSER_IDENTIFIERS), INIPARSER_PREFIXES,
     MAZEN_ARRAY_LEN(INIPARSER_PREFIXES)},
    {TOML_LIBS, MAZEN_ARRAY_LEN(TOML_LIBS), TOML_HEADERS, MAZEN_ARRAY_LEN(TOML_HEADERS), TOML_IDENTIFIERS,
     MAZEN_ARRAY_LEN(TOML_IDENTIFIERS), TOML_PREFIXES, MAZEN_ARRAY_LEN(TOML_PREFIXES)},
    {CIVETWEB_LIBS, MAZEN_ARRAY_LEN(CIVETWEB_LIBS), CIVETWEB_HEADERS, MAZEN_ARRAY_LEN(CIVETWEB_HEADERS),
     CIVETWEB_IDENTIFIERS, MAZEN_ARRAY_LEN(CIVETWEB_IDENTIFIERS), NULL, 0},
    {MONGOOSE_LIBS, MAZEN_ARRAY_LEN(MONGOOSE_LIBS), MONGOOSE_HEADERS, MAZEN_ARRAY_LEN(MONGOOSE_HEADERS),
     MONGOOSE_IDENTIFIERS, MAZEN_ARRAY_LEN(MONGOOSE_IDENTIFIERS), NULL, 0},
    {MHD_LIBS, MAZEN_ARRAY_LEN(MHD_LIBS), MHD_HEADERS, MAZEN_ARRAY_LEN(MHD_HEADERS), MHD_IDENTIFIERS,
     MAZEN_ARRAY_LEN(MHD_IDENTIFIERS), MHD_PREFIXES, MAZEN_ARRAY_LEN(MHD_PREFIXES)},
    {WEBSOCKETS_LIBS, MAZEN_ARRAY_LEN(WEBSOCKETS_LIBS), WEBSOCKETS_HEADERS, MAZEN_ARRAY_LEN(WEBSOCKETS_HEADERS),
     WEBSOCKETS_IDENTIFIERS, MAZEN_ARRAY_LEN(WEBSOCKETS_IDENTIFIERS), WEBSOCKETS_PREFIXES,
     MAZEN_ARRAY_LEN(WEBSOCKETS_PREFIXES)},
    {RABBITMQ_LIBS, MAZEN_ARRAY_LEN(RABBITMQ_LIBS), RABBITMQ_HEADERS, MAZEN_ARRAY_LEN(RABBITMQ_HEADERS),
     RABBITMQ_IDENTIFIERS, MAZEN_ARRAY_LEN(RABBITMQ_IDENTIFIERS), RABBITMQ_PREFIXES,
     MAZEN_ARRAY_LEN(RABBITMQ_PREFIXES)},
    {RDKAFKA_LIBS, MAZEN_ARRAY_LEN(RDKAFKA_LIBS), RDKAFKA_HEADERS, MAZEN_ARRAY_LEN(RDKAFKA_HEADERS),
     RDKAFKA_IDENTIFIERS, MAZEN_ARRAY_LEN(RDKAFKA_IDENTIFIERS), RDKAFKA_PREFIXES,
     MAZEN_ARRAY_LEN(RDKAFKA_PREFIXES)},
    {MODBUS_LIBS, MAZEN_ARRAY_LEN(MODBUS_LIBS), MODBUS_HEADERS, MAZEN_ARRAY_LEN(MODBUS_HEADERS),
     MODBUS_IDENTIFIERS, MAZEN_ARRAY_LEN(MODBUS_IDENTIFIERS), MODBUS_PREFIXES, MAZEN_ARRAY_LEN(MODBUS_PREFIXES)},
    {SERIALPORT_LIBS, MAZEN_ARRAY_LEN(SERIALPORT_LIBS), SERIALPORT_HEADERS, MAZEN_ARRAY_LEN(SERIALPORT_HEADERS),
     SERIALPORT_IDENTIFIERS, MAZEN_ARRAY_LEN(SERIALPORT_IDENTIFIERS), SERIALPORT_PREFIXES,
     MAZEN_ARRAY_LEN(SERIALPORT_PREFIXES)},
    {FTDI_LIBS, MAZEN_ARRAY_LEN(FTDI_LIBS), FTDI_HEADERS, MAZEN_ARRAY_LEN(FTDI_HEADERS), FTDI_IDENTIFIERS,
     MAZEN_ARRAY_LEN(FTDI_IDENTIFIERS), FTDI_PREFIXES, MAZEN_ARRAY_LEN(FTDI_PREFIXES)},
    {GPIOD_LIBS, MAZEN_ARRAY_LEN(GPIOD_LIBS), GPIOD_HEADERS, MAZEN_ARRAY_LEN(GPIOD_HEADERS), GPIOD_IDENTIFIERS,
     MAZEN_ARRAY_LEN(GPIOD_IDENTIFIERS), GPIOD_PREFIXES, MAZEN_ARRAY_LEN(GPIOD_PREFIXES)},
    {WIRINGPI_LIBS, MAZEN_ARRAY_LEN(WIRINGPI_LIBS), WIRINGPI_HEADERS, MAZEN_ARRAY_LEN(WIRINGPI_HEADERS),
     WIRINGPI_IDENTIFIERS, MAZEN_ARRAY_LEN(WIRINGPI_IDENTIFIERS), NULL, 0},
    {LIBINPUT_LIBS, MAZEN_ARRAY_LEN(LIBINPUT_LIBS), LIBINPUT_HEADERS, MAZEN_ARRAY_LEN(LIBINPUT_HEADERS),
     LIBINPUT_IDENTIFIERS, MAZEN_ARRAY_LEN(LIBINPUT_IDENTIFIERS), LIBINPUT_PREFIXES,
     MAZEN_ARRAY_LEN(LIBINPUT_PREFIXES)},
    {WAYLAND_CLIENT_LIBS, MAZEN_ARRAY_LEN(WAYLAND_CLIENT_LIBS), WAYLAND_CLIENT_HEADERS,
     MAZEN_ARRAY_LEN(WAYLAND_CLIENT_HEADERS), WAYLAND_CLIENT_IDENTIFIERS,
     MAZEN_ARRAY_LEN(WAYLAND_CLIENT_IDENTIFIERS), WAYLAND_CLIENT_PREFIXES,
     MAZEN_ARRAY_LEN(WAYLAND_CLIENT_PREFIXES)},
    {WAYLAND_SERVER_LIBS, MAZEN_ARRAY_LEN(WAYLAND_SERVER_LIBS), WAYLAND_SERVER_HEADERS,
     MAZEN_ARRAY_LEN(WAYLAND_SERVER_HEADERS), WAYLAND_SERVER_IDENTIFIERS,
     MAZEN_ARRAY_LEN(WAYLAND_SERVER_IDENTIFIERS), WAYLAND_SERVER_PREFIXES,
     MAZEN_ARRAY_LEN(WAYLAND_SERVER_PREFIXES)},
    {XKBCOMMON_LIBS, MAZEN_ARRAY_LEN(XKBCOMMON_LIBS), XKBCOMMON_HEADERS, MAZEN_ARRAY_LEN(XKBCOMMON_HEADERS),
     XKBCOMMON_IDENTIFIERS, MAZEN_ARRAY_LEN(XKBCOMMON_IDENTIFIERS), XKBCOMMON_PREFIXES,
     MAZEN_ARRAY_LEN(XKBCOMMON_PREFIXES)},
    {XCB_LIBS, MAZEN_ARRAY_LEN(XCB_LIBS), XCB_HEADERS, MAZEN_ARRAY_LEN(XCB_HEADERS), XCB_IDENTIFIERS,
     MAZEN_ARRAY_LEN(XCB_IDENTIFIERS), XCB_PREFIXES, MAZEN_ARRAY_LEN(XCB_PREFIXES)},
    {X11_LIBS, MAZEN_ARRAY_LEN(X11_LIBS), X11_HEADERS, MAZEN_ARRAY_LEN(X11_HEADERS), X11_IDENTIFIERS,
     MAZEN_ARRAY_LEN(X11_IDENTIFIERS), NULL, 0},
    {AUTO_LIBOGG_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBOGG_LIBS), AUTO_LIBOGG_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBOGG_HEADERS), AUTO_LIBOGG_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBOGG_IDENTIFIERS), NULL, 0},
    {AUTO_LIBVORBIS_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBVORBIS_LIBS), AUTO_LIBVORBIS_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBVORBIS_HEADERS), AUTO_LIBVORBIS_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBVORBIS_IDENTIFIERS), NULL, 0},
    {AUTO_LIBTHEORA_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBTHEORA_LIBS), AUTO_LIBTHEORA_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBTHEORA_HEADERS), AUTO_LIBTHEORA_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBTHEORA_IDENTIFIERS), NULL, 0},
    {AUTO_LIBOPUS_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBOPUS_LIBS), AUTO_LIBOPUS_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBOPUS_HEADERS), AUTO_LIBOPUS_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBOPUS_IDENTIFIERS), NULL, 0},
    {AUTO_OPUSFILE_LIBS, MAZEN_ARRAY_LEN(AUTO_OPUSFILE_LIBS), AUTO_OPUSFILE_HEADERS, MAZEN_ARRAY_LEN(AUTO_OPUSFILE_HEADERS), AUTO_OPUSFILE_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_OPUSFILE_IDENTIFIERS), NULL, 0},
    {AUTO_LIBFLAC_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBFLAC_LIBS), AUTO_LIBFLAC_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBFLAC_HEADERS), AUTO_LIBFLAC_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBFLAC_IDENTIFIERS), NULL, 0},
    {AUTO_LIBSNDFILE_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBSNDFILE_LIBS), AUTO_LIBSNDFILE_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBSNDFILE_HEADERS), AUTO_LIBSNDFILE_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBSNDFILE_IDENTIFIERS), NULL, 0},
    {AUTO_MPG123_LIBS, MAZEN_ARRAY_LEN(AUTO_MPG123_LIBS), AUTO_MPG123_HEADERS, MAZEN_ARRAY_LEN(AUTO_MPG123_HEADERS), AUTO_MPG123_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_MPG123_IDENTIFIERS), NULL, 0},
    {AUTO_LIBMPG123_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBMPG123_LIBS), AUTO_LIBMPG123_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBMPG123_HEADERS), AUTO_LIBMPG123_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBMPG123_IDENTIFIERS), NULL, 0},
    {AUTO_LAME_LIBS, MAZEN_ARRAY_LEN(AUTO_LAME_LIBS), AUTO_LAME_HEADERS, MAZEN_ARRAY_LEN(AUTO_LAME_HEADERS), AUTO_LAME_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LAME_IDENTIFIERS), NULL, 0},
    {AUTO_LIBAVCODEC_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBAVCODEC_LIBS), AUTO_LIBAVCODEC_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBAVCODEC_HEADERS), AUTO_LIBAVCODEC_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBAVCODEC_IDENTIFIERS), NULL, 0},
    {AUTO_LIBAVFORMAT_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBAVFORMAT_LIBS), AUTO_LIBAVFORMAT_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBAVFORMAT_HEADERS), AUTO_LIBAVFORMAT_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBAVFORMAT_IDENTIFIERS), NULL, 0},
    {AUTO_LIBAVUTIL_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBAVUTIL_LIBS), AUTO_LIBAVUTIL_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBAVUTIL_HEADERS), AUTO_LIBAVUTIL_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBAVUTIL_IDENTIFIERS), NULL, 0},
    {AUTO_LIBSWSCALE_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBSWSCALE_LIBS), AUTO_LIBSWSCALE_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBSWSCALE_HEADERS), AUTO_LIBSWSCALE_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBSWSCALE_IDENTIFIERS), NULL, 0},
    {AUTO_LIBSWRESAMPLE_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBSWRESAMPLE_LIBS), AUTO_LIBSWRESAMPLE_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBSWRESAMPLE_HEADERS), AUTO_LIBSWRESAMPLE_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBSWRESAMPLE_IDENTIFIERS), NULL, 0},
    {AUTO_LIBPOSTPROC_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBPOSTPROC_LIBS), AUTO_LIBPOSTPROC_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBPOSTPROC_HEADERS), AUTO_LIBPOSTPROC_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBPOSTPROC_IDENTIFIERS), NULL, 0},
    {AUTO_FFMPEG_LIBRARIES_LIBS, MAZEN_ARRAY_LEN(AUTO_FFMPEG_LIBRARIES_LIBS), NULL, 0, NULL, 0, NULL, 0},
    {AUTO_GSTREAMER_LIBS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_LIBS), AUTO_GSTREAMER_HEADERS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_HEADERS), AUTO_GSTREAMER_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_IDENTIFIERS), NULL, 0},
    {AUTO_GSTREAMER_BASE_LIBS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_BASE_LIBS), AUTO_GSTREAMER_BASE_HEADERS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_BASE_HEADERS), NULL, 0, NULL, 0},
    {AUTO_GSTREAMER_PLUGINS_BASE_LIBS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_PLUGINS_BASE_LIBS), AUTO_GSTREAMER_PLUGINS_BASE_HEADERS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_PLUGINS_BASE_HEADERS), NULL, 0, NULL, 0},
    {AUTO_GSTREAMER_PLUGINS_GOOD_LIBS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_PLUGINS_GOOD_LIBS), AUTO_GSTREAMER_PLUGINS_GOOD_HEADERS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_PLUGINS_GOOD_HEADERS), NULL, 0, NULL, 0},
    {AUTO_GSTREAMER_PLUGINS_BAD_LIBS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_PLUGINS_BAD_LIBS), AUTO_GSTREAMER_PLUGINS_BAD_HEADERS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_PLUGINS_BAD_HEADERS), NULL, 0, NULL, 0},
    {AUTO_GSTREAMER_PLUGINS_UGLY_LIBS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_PLUGINS_UGLY_LIBS), AUTO_GSTREAMER_PLUGINS_UGLY_HEADERS, MAZEN_ARRAY_LEN(AUTO_GSTREAMER_PLUGINS_UGLY_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBMATROSKA_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBMATROSKA_LIBS), AUTO_LIBMATROSKA_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBMATROSKA_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBEBML_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBEBML_LIBS), AUTO_LIBEBML_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBEBML_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBVPX_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBVPX_LIBS), AUTO_LIBVPX_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBVPX_HEADERS), NULL, 0, NULL, 0},
    {AUTO_DAV1D_LIBS, MAZEN_ARRAY_LEN(AUTO_DAV1D_LIBS), AUTO_DAV1D_HEADERS, MAZEN_ARRAY_LEN(AUTO_DAV1D_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBAOM_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBAOM_LIBS), AUTO_LIBAOM_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBAOM_HEADERS), NULL, 0, NULL, 0},
    {AUTO_X264_LIBS, MAZEN_ARRAY_LEN(AUTO_X264_LIBS), AUTO_X264_HEADERS, MAZEN_ARRAY_LEN(AUTO_X264_HEADERS), NULL, 0, NULL, 0},
    {AUTO_X265_LIBS, MAZEN_ARRAY_LEN(AUTO_X265_LIBS), AUTO_X265_HEADERS, MAZEN_ARRAY_LEN(AUTO_X265_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBHEIF_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBHEIF_LIBS), AUTO_LIBHEIF_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBHEIF_HEADERS), NULL, 0, NULL, 0},
    {AUTO_OPENEXR_LIBS, MAZEN_ARRAY_LEN(AUTO_OPENEXR_LIBS), AUTO_OPENEXR_HEADERS, MAZEN_ARRAY_LEN(AUTO_OPENEXR_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBTIFF_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBTIFF_LIBS), AUTO_LIBTIFF_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBTIFF_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBWEBP_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBWEBP_LIBS), AUTO_LIBWEBP_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBWEBP_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBOPENJP2_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBOPENJP2_LIBS), AUTO_LIBOPENJP2_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBOPENJP2_HEADERS), NULL, 0, NULL, 0},
    {AUTO_JASPER_LIBS, MAZEN_ARRAY_LEN(AUTO_JASPER_LIBS), AUTO_JASPER_HEADERS, MAZEN_ARRAY_LEN(AUTO_JASPER_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBRAW_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBRAW_LIBS), AUTO_LIBRAW_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBRAW_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LCMS2_LIBS, MAZEN_ARRAY_LEN(AUTO_LCMS2_LIBS), AUTO_LCMS2_HEADERS, MAZEN_ARRAY_LEN(AUTO_LCMS2_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBEXIF_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBEXIF_LIBS), AUTO_LIBEXIF_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBEXIF_HEADERS), NULL, 0, NULL, 0},
    {AUTO_EXIV2_LIBS, MAZEN_ARRAY_LEN(AUTO_EXIV2_LIBS), AUTO_EXIV2_HEADERS, MAZEN_ARRAY_LEN(AUTO_EXIV2_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBSPNG_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBSPNG_LIBS), AUTO_LIBSPNG_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBSPNG_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBGD_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBGD_LIBS), AUTO_LIBGD_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBGD_HEADERS), NULL, 0, NULL, 0},
    {AUTO_FREETYPE_GL_LIBS, MAZEN_ARRAY_LEN(AUTO_FREETYPE_GL_LIBS), AUTO_FREETYPE_GL_HEADERS, MAZEN_ARRAY_LEN(AUTO_FREETYPE_GL_HEADERS), NULL, 0, NULL, 0},
    {AUTO_GLFW_LIBS, MAZEN_ARRAY_LEN(AUTO_GLFW_LIBS), AUTO_GLFW_HEADERS, MAZEN_ARRAY_LEN(AUTO_GLFW_HEADERS), AUTO_GLFW_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_GLFW_IDENTIFIERS), NULL, 0},
    {AUTO_GLEW_LIBS, MAZEN_ARRAY_LEN(AUTO_GLEW_LIBS), AUTO_GLEW_HEADERS, MAZEN_ARRAY_LEN(AUTO_GLEW_HEADERS), AUTO_GLEW_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_GLEW_IDENTIFIERS), NULL, 0},
    {AUTO_GLAD_LIBS, MAZEN_ARRAY_LEN(AUTO_GLAD_LIBS), AUTO_GLAD_HEADERS, MAZEN_ARRAY_LEN(AUTO_GLAD_HEADERS), AUTO_GLAD_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_GLAD_IDENTIFIERS), NULL, 0},
    {AUTO_MESA_LIBS, MAZEN_ARRAY_LEN(AUTO_MESA_LIBS), AUTO_MESA_HEADERS, MAZEN_ARRAY_LEN(AUTO_MESA_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBEPOXY_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBEPOXY_LIBS), AUTO_LIBEPOXY_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBEPOXY_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBASS_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBASS_LIBS), AUTO_LIBASS_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBASS_HEADERS), AUTO_LIBASS_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBASS_IDENTIFIERS), NULL, 0},
    {AUTO_HARFBUZZ_SUBSET_LIBS, MAZEN_ARRAY_LEN(AUTO_HARFBUZZ_SUBSET_LIBS), AUTO_HARFBUZZ_SUBSET_HEADERS, MAZEN_ARRAY_LEN(AUTO_HARFBUZZ_SUBSET_HEADERS), NULL, 0, NULL, 0},
    {AUTO_ICU_LIBS, MAZEN_ARRAY_LEN(AUTO_ICU_LIBS), AUTO_ICU_HEADERS, MAZEN_ARRAY_LEN(AUTO_ICU_HEADERS), AUTO_ICU_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_ICU_IDENTIFIERS), NULL, 0},
    {AUTO_LIBIDN_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBIDN_LIBS), AUTO_LIBIDN_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBIDN_HEADERS), AUTO_LIBIDN_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBIDN_IDENTIFIERS), NULL, 0},
    {AUTO_LIBIDN2_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBIDN2_LIBS), AUTO_LIBIDN2_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBIDN2_HEADERS), AUTO_LIBIDN2_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBIDN2_IDENTIFIERS), NULL, 0},
    {AUTO_LIBPSL_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBPSL_LIBS), AUTO_LIBPSL_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBPSL_HEADERS), AUTO_LIBPSL_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBPSL_IDENTIFIERS), NULL, 0},
    {AUTO_LIBTASN1_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBTASN1_LIBS), AUTO_LIBTASN1_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBTASN1_HEADERS), AUTO_LIBTASN1_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBTASN1_IDENTIFIERS), NULL, 0},
    {AUTO_LIBUNISTRING_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBUNISTRING_LIBS), AUTO_LIBUNISTRING_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBUNISTRING_HEADERS), AUTO_LIBUNISTRING_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBUNISTRING_IDENTIFIERS), NULL, 0},
    {AUTO_LIBICONV_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBICONV_LIBS), AUTO_LIBICONV_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBICONV_HEADERS), AUTO_LIBICONV_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBICONV_IDENTIFIERS), NULL, 0},
    {AUTO_LIBINTL_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBINTL_LIBS), AUTO_LIBINTL_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBINTL_HEADERS), AUTO_LIBINTL_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBINTL_IDENTIFIERS), NULL, 0},
    {AUTO_LIBPCRE_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBPCRE_LIBS), AUTO_LIBPCRE_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBPCRE_HEADERS), AUTO_LIBPCRE_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBPCRE_IDENTIFIERS), NULL, 0},
    {AUTO_PCRE2_LIBS, MAZEN_ARRAY_LEN(AUTO_PCRE2_LIBS), AUTO_PCRE2_HEADERS, MAZEN_ARRAY_LEN(AUTO_PCRE2_HEADERS), AUTO_PCRE2_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_PCRE2_IDENTIFIERS), NULL, 0},
    {AUTO_RE2C_LIBS, MAZEN_ARRAY_LEN(AUTO_RE2C_LIBS), AUTO_RE2C_HEADERS, MAZEN_ARRAY_LEN(AUTO_RE2C_HEADERS), NULL, 0, NULL, 0},
    {AUTO_ONIGURUMA_LIBS, MAZEN_ARRAY_LEN(AUTO_ONIGURUMA_LIBS), AUTO_ONIGURUMA_HEADERS, MAZEN_ARRAY_LEN(AUTO_ONIGURUMA_HEADERS), AUTO_ONIGURUMA_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_ONIGURUMA_IDENTIFIERS), NULL, 0},
    {AUTO_LIBXMLB_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBXMLB_LIBS), AUTO_LIBXMLB_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBXMLB_HEADERS), NULL, 0, NULL, 0},
    {AUTO_EXPAT_LIBS, MAZEN_ARRAY_LEN(AUTO_EXPAT_LIBS), AUTO_EXPAT_HEADERS, MAZEN_ARRAY_LEN(AUTO_EXPAT_HEADERS), AUTO_EXPAT_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_EXPAT_IDENTIFIERS), NULL, 0},
    {AUTO_TINYXML2_LIBS, MAZEN_ARRAY_LEN(AUTO_TINYXML2_LIBS), AUTO_TINYXML2_HEADERS, MAZEN_ARRAY_LEN(AUTO_TINYXML2_HEADERS), AUTO_TINYXML2_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_TINYXML2_IDENTIFIERS), NULL, 0},
    {AUTO_RAPIDXML_LIBS, MAZEN_ARRAY_LEN(AUTO_RAPIDXML_LIBS), AUTO_RAPIDXML_HEADERS, MAZEN_ARRAY_LEN(AUTO_RAPIDXML_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBFASTJSON_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBFASTJSON_LIBS), AUTO_LIBFASTJSON_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBFASTJSON_HEADERS), NULL, 0, NULL, 0},
    {AUTO_YYJSON_LIBS, MAZEN_ARRAY_LEN(AUTO_YYJSON_LIBS), AUTO_YYJSON_HEADERS, MAZEN_ARRAY_LEN(AUTO_YYJSON_HEADERS), AUTO_YYJSON_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_YYJSON_IDENTIFIERS), NULL, 0},
    {AUTO_SIMDJSON_LIBS, MAZEN_ARRAY_LEN(AUTO_SIMDJSON_LIBS), AUTO_SIMDJSON_HEADERS, MAZEN_ARRAY_LEN(AUTO_SIMDJSON_HEADERS), AUTO_SIMDJSON_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_SIMDJSON_IDENTIFIERS), NULL, 0},
    {AUTO_LIBCSV_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBCSV_LIBS), AUTO_LIBCSV_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBCSV_HEADERS), AUTO_LIBCSV_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBCSV_IDENTIFIERS), NULL, 0},
    {AUTO_LIBXLSXWRITER_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBXLSXWRITER_LIBS), AUTO_LIBXLSXWRITER_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBXLSXWRITER_HEADERS), AUTO_LIBXLSXWRITER_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBXLSXWRITER_IDENTIFIERS), NULL, 0},
    {AUTO_LIBHARU_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBHARU_LIBS), AUTO_LIBHARU_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBHARU_HEADERS), AUTO_LIBHARU_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBHARU_IDENTIFIERS), NULL, 0},
    {AUTO_MUPDF_LIBS, MAZEN_ARRAY_LEN(AUTO_MUPDF_LIBS), AUTO_MUPDF_HEADERS, MAZEN_ARRAY_LEN(AUTO_MUPDF_HEADERS), AUTO_MUPDF_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_MUPDF_IDENTIFIERS), NULL, 0},
    {AUTO_POPPLER_LIBS, MAZEN_ARRAY_LEN(AUTO_POPPLER_LIBS), AUTO_POPPLER_HEADERS, MAZEN_ARRAY_LEN(AUTO_POPPLER_HEADERS), AUTO_POPPLER_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_POPPLER_IDENTIFIERS), NULL, 0},
    {AUTO_QPDF_LIBS, MAZEN_ARRAY_LEN(AUTO_QPDF_LIBS), AUTO_QPDF_HEADERS, MAZEN_ARRAY_LEN(AUTO_QPDF_HEADERS), AUTO_QPDF_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_QPDF_IDENTIFIERS), NULL, 0},
    {AUTO_LIBARCHIVE_C_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBARCHIVE_C_LIBS), AUTO_LIBARCHIVE_C_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBARCHIVE_C_HEADERS), NULL, 0, NULL, 0},
    {AUTO_MINIZ_LIBS, MAZEN_ARRAY_LEN(AUTO_MINIZ_LIBS), AUTO_MINIZ_HEADERS, MAZEN_ARRAY_LEN(AUTO_MINIZ_HEADERS), AUTO_MINIZ_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_MINIZ_IDENTIFIERS), NULL, 0},
    {AUTO_LZ4_LIBS, MAZEN_ARRAY_LEN(AUTO_LZ4_LIBS), AUTO_LZ4_HEADERS, MAZEN_ARRAY_LEN(AUTO_LZ4_HEADERS), AUTO_LZ4_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LZ4_IDENTIFIERS), NULL, 0},
    {AUTO_ZSTD_LIBS, MAZEN_ARRAY_LEN(AUTO_ZSTD_LIBS), AUTO_ZSTD_HEADERS, MAZEN_ARRAY_LEN(AUTO_ZSTD_HEADERS), AUTO_ZSTD_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_ZSTD_IDENTIFIERS), NULL, 0},
    {AUTO_BROTLI_LIBS, MAZEN_ARRAY_LEN(AUTO_BROTLI_LIBS), AUTO_BROTLI_HEADERS, MAZEN_ARRAY_LEN(AUTO_BROTLI_HEADERS), AUTO_BROTLI_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_BROTLI_IDENTIFIERS), NULL, 0},
    {AUTO_LIBDEFLATE_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBDEFLATE_LIBS), AUTO_LIBDEFLATE_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBDEFLATE_HEADERS), AUTO_LIBDEFLATE_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBDEFLATE_IDENTIFIERS), NULL, 0},
    {AUTO_SNAPPY_LIBS, MAZEN_ARRAY_LEN(AUTO_SNAPPY_LIBS), AUTO_SNAPPY_HEADERS, MAZEN_ARRAY_LEN(AUTO_SNAPPY_HEADERS), AUTO_SNAPPY_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_SNAPPY_IDENTIFIERS), NULL, 0},
    {AUTO_ISA_L_LIBS, MAZEN_ARRAY_LEN(AUTO_ISA_L_LIBS), AUTO_ISA_L_HEADERS, MAZEN_ARRAY_LEN(AUTO_ISA_L_HEADERS), NULL, 0, NULL, 0},
    {AUTO_TBBMALLOC_LIBS, MAZEN_ARRAY_LEN(AUTO_TBBMALLOC_LIBS), AUTO_TBBMALLOC_HEADERS, MAZEN_ARRAY_LEN(AUTO_TBBMALLOC_HEADERS), NULL, 0, NULL, 0},
    {AUTO_JEMALLOC_LIBS, MAZEN_ARRAY_LEN(AUTO_JEMALLOC_LIBS), AUTO_JEMALLOC_HEADERS, MAZEN_ARRAY_LEN(AUTO_JEMALLOC_HEADERS), AUTO_JEMALLOC_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_JEMALLOC_IDENTIFIERS), NULL, 0},
    {AUTO_TCMALLOC_LIBS, MAZEN_ARRAY_LEN(AUTO_TCMALLOC_LIBS), AUTO_TCMALLOC_HEADERS, MAZEN_ARRAY_LEN(AUTO_TCMALLOC_HEADERS), NULL, 0, NULL, 0},
    {AUTO_MIMALLOC_LIBS, MAZEN_ARRAY_LEN(AUTO_MIMALLOC_LIBS), AUTO_MIMALLOC_HEADERS, MAZEN_ARRAY_LEN(AUTO_MIMALLOC_HEADERS), AUTO_MIMALLOC_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_MIMALLOC_IDENTIFIERS), NULL, 0},
    {AUTO_BOEHM_GC_LIBS, MAZEN_ARRAY_LEN(AUTO_BOEHM_GC_LIBS), AUTO_BOEHM_GC_HEADERS, MAZEN_ARRAY_LEN(AUTO_BOEHM_GC_HEADERS), AUTO_BOEHM_GC_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_BOEHM_GC_IDENTIFIERS), NULL, 0},
    {AUTO_LIBUVC_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBUVC_LIBS), AUTO_LIBUVC_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBUVC_HEADERS), AUTO_LIBUVC_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBUVC_IDENTIFIERS), NULL, 0},
    {AUTO_LIBCAMERA_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBCAMERA_LIBS), AUTO_LIBCAMERA_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBCAMERA_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBFREENECT_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBFREENECT_LIBS), AUTO_LIBFREENECT_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBFREENECT_HEADERS), AUTO_LIBFREENECT_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBFREENECT_IDENTIFIERS), NULL, 0},
    {AUTO_LIBREALSENSE_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBREALSENSE_LIBS), AUTO_LIBREALSENSE_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBREALSENSE_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBHIDAPI_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBHIDAPI_LIBS), AUTO_LIBHIDAPI_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBHIDAPI_HEADERS), AUTO_LIBHIDAPI_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBHIDAPI_IDENTIFIERS), NULL, 0},
    {AUTO_LIBIIO_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBIIO_LIBS), AUTO_LIBIIO_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBIIO_HEADERS), AUTO_LIBIIO_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBIIO_IDENTIFIERS), NULL, 0},
    {AUTO_LIBFTD2XX_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBFTD2XX_LIBS), AUTO_LIBFTD2XX_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBFTD2XX_HEADERS), NULL, 0, NULL, 0},
    {AUTO_LIBGPHOTO2_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBGPHOTO2_LIBS), AUTO_LIBGPHOTO2_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBGPHOTO2_HEADERS), AUTO_LIBGPHOTO2_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBGPHOTO2_IDENTIFIERS), NULL, 0},
    {AUTO_LIBMTP_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBMTP_LIBS), AUTO_LIBMTP_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBMTP_HEADERS), AUTO_LIBMTP_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBMTP_IDENTIFIERS), NULL, 0},
    {AUTO_LIBBLURAY_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBBLURAY_LIBS), AUTO_LIBBLURAY_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBBLURAY_HEADERS), AUTO_LIBBLURAY_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBBLURAY_IDENTIFIERS), NULL, 0},
    {AUTO_LIBDVDNAV_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBDVDNAV_LIBS), AUTO_LIBDVDNAV_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBDVDNAV_HEADERS), AUTO_LIBDVDNAV_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBDVDNAV_IDENTIFIERS), NULL, 0},
    {AUTO_LIBDVDREAD_LIBS, MAZEN_ARRAY_LEN(AUTO_LIBDVDREAD_LIBS), AUTO_LIBDVDREAD_HEADERS, MAZEN_ARRAY_LEN(AUTO_LIBDVDREAD_HEADERS), AUTO_LIBDVDREAD_IDENTIFIERS, MAZEN_ARRAY_LEN(AUTO_LIBDVDREAD_IDENTIFIERS), NULL, 0},
};

static bool is_identifier_char(char ch) {
    return isalnum((unsigned char) ch) || ch == '_';
}

static char *strip_comments_and_literals(const char *content) {
    String out;
    size_t i = 0;
    enum {
        STATE_NORMAL,
        STATE_SLASH,
        STATE_LINE_COMMENT,
        STATE_BLOCK_COMMENT,
        STATE_BLOCK_COMMENT_STAR,
        STATE_STRING,
        STATE_STRING_ESCAPE,
        STATE_CHAR,
        STATE_CHAR_ESCAPE
    } state = STATE_NORMAL;

    string_init(&out);
    while (content[i] != '\0') {
        char ch = content[i++];

        switch (state) {
        case STATE_NORMAL:
            if (ch == '/') {
                state = STATE_SLASH;
            } else if (ch == '"') {
                string_append_char(&out, ' ');
                state = STATE_STRING;
            } else if (ch == '\'') {
                string_append_char(&out, ' ');
                state = STATE_CHAR;
            } else {
                string_append_char(&out, ch);
            }
            break;
        case STATE_SLASH:
            if (ch == '/') {
                string_append_char(&out, ' ');
                string_append_char(&out, ' ');
                state = STATE_LINE_COMMENT;
            } else if (ch == '*') {
                string_append_char(&out, ' ');
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT;
            } else {
                string_append_char(&out, '/');
                string_append_char(&out, ch);
                state = STATE_NORMAL;
            }
            break;
        case STATE_LINE_COMMENT:
            string_append_char(&out, ch == '\n' ? '\n' : ' ');
            if (ch == '\n') {
                state = STATE_NORMAL;
            }
            break;
        case STATE_BLOCK_COMMENT:
            if (ch == '*') {
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT_STAR;
            } else if (ch == '\n') {
                string_append_char(&out, '\n');
            } else {
                string_append_char(&out, ' ');
            }
            break;
        case STATE_BLOCK_COMMENT_STAR:
            if (ch == '/') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else if (ch == '\n') {
                string_append_char(&out, '\n');
                state = STATE_BLOCK_COMMENT;
            } else if (ch == '*') {
                string_append_char(&out, ' ');
            } else {
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT;
            }
            break;
        case STATE_STRING:
            if (ch == '\\') {
                string_append_char(&out, ' ');
                state = STATE_STRING_ESCAPE;
            } else if (ch == '"') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else {
                string_append_char(&out, ch == '\n' ? '\n' : ' ');
            }
            break;
        case STATE_STRING_ESCAPE:
            string_append_char(&out, ch == '\n' ? '\n' : ' ');
            state = STATE_STRING;
            break;
        case STATE_CHAR:
            if (ch == '\\') {
                string_append_char(&out, ' ');
                state = STATE_CHAR_ESCAPE;
            } else if (ch == '\'') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else {
                string_append_char(&out, ch == '\n' ? '\n' : ' ');
            }
            break;
        case STATE_CHAR_ESCAPE:
            string_append_char(&out, ch == '\n' ? '\n' : ' ');
            state = STATE_CHAR;
            break;
        }
    }

    if (state == STATE_SLASH) {
        string_append_char(&out, '/');
    }
    return string_take(&out);
}

static void collect_includes(const char *content, StringList *includes, StringList *system_includes) {
    const char *cursor = content;

    while (*cursor != '\0') {
        while (*cursor != '\0' && *cursor != '#') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        ++cursor;
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        if (strncmp(cursor, "include", 7) != 0 || is_identifier_char(cursor[7])) {
            while (*cursor != '\0' && *cursor != '\n') {
                ++cursor;
            }
            continue;
        }
        cursor += 7;
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        if (*cursor == '"' || *cursor == '<') {
            char opener = *cursor;
            char terminator = opener == '"' ? '"' : '>';
            String include;

            string_init(&include);
            ++cursor;
            while (*cursor != '\0' && *cursor != terminator && *cursor != '\n') {
                string_append_char(&include, *cursor++);
            }
            if (*cursor == terminator) {
                char *path = string_take(&include);
                string_list_push_unique(includes, path);
                if (opener == '<') {
                    string_list_push_unique(system_includes, path);
                }
                free(path);
            } else {
                string_free(&include);
            }
        }
        while (*cursor != '\0' && *cursor != '\n') {
            ++cursor;
        }
    }
}

static bool contains_identifier(const char *text, const char *identifier) {
    size_t len = strlen(identifier);
    const char *cursor = text;

    while ((cursor = strstr(cursor, identifier)) != NULL) {
        char before = cursor == text ? '\0' : cursor[-1];
        char after = cursor[len];
        if (!is_identifier_char(before) && !is_identifier_char(after)) {
            return true;
        }
        ++cursor;
    }
    return false;
}

static bool contains_identifier_prefix(const char *text, const char *prefix) {
    size_t len = strlen(prefix);
    const char *cursor = text;

    while ((cursor = strstr(cursor, prefix)) != NULL) {
        char before = cursor == text ? '\0' : cursor[-1];
        if (!is_identifier_char(before) && is_identifier_char(cursor[len])) {
            return true;
        }
        ++cursor;
    }
    return false;
}

static bool includes_header(const StringList *includes, const char *header) {
    size_t i;

    for (i = 0; i < includes->len; ++i) {
        if (strcmp(includes->items[i], header) == 0) {
            return true;
        }
    }
    return false;
}



static bool is_pkg_name_char(char ch) {
    return isalnum((unsigned char) ch) || ch == '_' || ch == '-' || ch == '.' || ch == '+';
}

static bool text_is_safe_pkg_name(const char *text) {
    const char *start;
    const char *end;
    const char *cursor;

    if (text == NULL) {
        return false;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char) *start)) {
        ++start;
    }
    if (*start == '\0' || *start == '-') {
        return false;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char) end[-1])) {
        --end;
    }

    cursor = start;
    while (cursor < end) {
        if (!is_pkg_name_char(*cursor)) {
            return false;
        }
        ++cursor;
    }
    return true;
}

static bool pkg_config_exists(void) {
    static int cached = -1;

    if (cached < 0) {
        cached = system("command -v pkg-config >/dev/null 2>&1") == 0 ? 1 : 0;
    }
    return cached == 1;
}

typedef struct {
    char *package;
    StringList libs;
    bool queried;
} PkgLibMemoEntry;

typedef struct {
    PkgLibMemoEntry *items;
    size_t len;
    size_t cap;
} PkgLibMemo;

static PkgLibMemoEntry *pkg_lib_memo_get_or_create(PkgLibMemo *memo, const char *package) {
    size_t i;

    for (i = 0; i < memo->len; ++i) {
        if (strcmp(memo->items[i].package, package) == 0) {
            return &memo->items[i];
        }
    }

    if (memo->len == memo->cap) {
        size_t new_cap = memo->cap == 0 ? 16 : memo->cap * 2;
        memo->items = mazen_xrealloc(memo->items, new_cap * sizeof(*memo->items));
        memo->cap = new_cap;
    }

    {
        PkgLibMemoEntry *entry = &memo->items[memo->len++];
        entry->package = mazen_xstrdup(package);
        string_list_init(&entry->libs);
        entry->queried = false;
        return entry;
    }
}

static void add_pkgconfig_libs(const char *package, StringList *libs) {
    static PkgLibMemo memo = {0};
    char command[512];
    FILE *pipe;
    char buffer[4096];
    PkgLibMemoEntry *entry;
    size_t i;

    if (!pkg_config_exists() || !text_is_safe_pkg_name(package)) {
        return;
    }

    entry = pkg_lib_memo_get_or_create(&memo, package);
    if (!entry->queried) {
        snprintf(command, sizeof(command), "pkg-config --exists %s >/dev/null 2>&1", package);
        if (system(command) == 0) {
            snprintf(command, sizeof(command), "pkg-config --libs %s 2>/dev/null", package);
            pipe = popen(command, "r");
            if (pipe != NULL) {
                while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                    char *cursor = buffer;

                    while (*cursor != '\0') {
                        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
                            ++cursor;
                        }
                        if (cursor[0] == '-' && cursor[1] == 'l') {
                            char *name = cursor + 2;
                            char saved;

                            while (*cursor != '\0' && !isspace((unsigned char) *cursor)) {
                                ++cursor;
                            }
                            saved = *cursor;
                            *cursor = '\0';
                            if (*name != '\0') {
                                string_list_push_unique(&entry->libs, name);
                            }
                            *cursor = saved;
                        } else {
                            while (*cursor != '\0' && !isspace((unsigned char) *cursor)) {
                                ++cursor;
                            }
                        }
                    }
                }
                pclose(pipe);
            }
        }
        entry->queried = true;
    }

    for (i = 0; i < entry->libs.len; ++i) {
        string_list_push_unique(libs, entry->libs.items[i]);
    }
}

static void normalize_pkg_token(const char *text, char *out, size_t out_cap) {
    size_t i = 0;

    if (out_cap == 0) {
        return;
    }
    if (text == NULL) {
        out[0] = '\0';
        return;
    }

    while (text[i] != '\0' && i + 1 < out_cap) {
        if (!is_pkg_name_char(text[i])) {
            break;
        }
        out[i] = (char) tolower((unsigned char) text[i]);
        ++i;
    }
    out[i] = '\0';
}

static bool pkg_token_is_too_generic(const char *token) {
    return token[0] == '\0' || strlen(token) < 3 || strcmp(token, "sys") == 0 || strcmp(token, "linux") == 0 ||
           strcmp(token, "bits") == 0 || strcmp(token, "usr") == 0 || strcmp(token, "include") == 0;
}

static bool package_matches_token(const char *package, const char *token) {
    char norm_pkg[256];
    size_t token_len;

    normalize_pkg_token(package, norm_pkg, sizeof(norm_pkg));
    if (norm_pkg[0] == '\0') {
        return false;
    }

    token_len = strlen(token);
    if (strcmp(norm_pkg, token) == 0) {
        return true;
    }
    if (string_starts_with(norm_pkg, token)) {
        char boundary = norm_pkg[token_len];
        if (boundary == '\0' || boundary == '-' || boundary == '.') {
            return true;
        }
    }
    if (string_starts_with(norm_pkg, "lib") && strcmp(norm_pkg + 3, token) == 0) {
        return true;
    }
    return false;
}

static void pkg_config_collect_packages(StringList *packages) {
    FILE *pipe;
    char line[4096];

    string_list_clear(packages);
    if (!pkg_config_exists()) {
        return;
    }

    pipe = popen("pkg-config --list-all 2>/dev/null", "r");
    if (pipe == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), pipe) != NULL) {
        char *cursor = line;
        char *token_start;
        char saved;

        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            continue;
        }

        token_start = cursor;
        while (*cursor != '\0' && !isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        saved = *cursor;
        *cursor = '\0';
        if (text_is_safe_pkg_name(token_start)) {
            string_list_push_unique(packages, token_start);
        }
        *cursor = saved;
    }

    pclose(pipe);
}

static void maybe_add_pkgconfig_token(const char *token, StringList *libs) {
    static StringList packages;
    static bool packages_init = false;
    static bool packages_loaded = false;
    static StringList token_cache;
    static bool token_cache_init = false;
    char normalized[256];
    size_t i;
    int fuzzy_hits = 0;

    normalize_pkg_token(token, normalized, sizeof(normalized));
    if (pkg_token_is_too_generic(normalized)) {
        return;
    }

    if (!token_cache_init) {
        string_list_init(&token_cache);
        token_cache_init = true;
    }
    if (string_list_contains(&token_cache, normalized)) {
        return;
    }
    string_list_push(&token_cache, normalized);

    add_pkgconfig_libs(normalized, libs);
    if (string_starts_with(normalized, "lib") && strlen(normalized) > 3) {
        add_pkgconfig_libs(normalized + 3, libs);
    }

    if (!pkg_config_exists()) {
        return;
    }

    if (!packages_init) {
        string_list_init(&packages);
        packages_init = true;
    }
    if (!packages_loaded) {
        pkg_config_collect_packages(&packages);
        packages_loaded = true;
    }

    for (i = 0; i < packages.len; ++i) {
        if (package_matches_token(packages.items[i], normalized)) {
            add_pkgconfig_libs(packages.items[i], libs);
            ++fuzzy_hits;
            if (fuzzy_hits >= 6) {
                break;
            }
        }
    }
}

static void maybe_add_pkgconfig_from_include(const char *include, StringList *libs) {
    const char *slash;
    char token[256];

    if (include == NULL || *include == '\0') {
        return;
    }

    slash = strchr(include, '/');
    if (slash != NULL) {
        size_t head_len = (size_t) (slash - include);
        if (head_len > 0 && head_len < sizeof(token)) {
            memcpy(token, include, head_len);
            token[head_len] = '\0';
            maybe_add_pkgconfig_token(token, libs);
        }
        maybe_add_pkgconfig_token(slash + 1, libs);
        return;
    }

    maybe_add_pkgconfig_token(include, libs);
}

static bool rule_matches(const AutoLibRule *rule, const StringList *includes, const char *text, bool allow_symbol_match) {
    size_t i;

    for (i = 0; i < rule->header_count; ++i) {
        if (includes_header(includes, rule->headers[i])) {
            return true;
        }
    }
    if (!allow_symbol_match) {
        return false;
    }
    for (i = 0; i < rule->identifier_count; ++i) {
        if (contains_identifier(text, rule->identifiers[i])) {
            return true;
        }
    }
    for (i = 0; i < rule->prefix_count; ++i) {
        if (contains_identifier_prefix(text, rule->prefixes[i])) {
            return true;
        }
    }
    return false;
}

static void add_rule_libs(const AutoLibRule *rule, StringList *libs) {
    size_t i;

    for (i = 0; i < rule->lib_count; ++i) {
        string_list_push_unique(libs, rule->libs[i]);
    }
}

void autolib_infer(const char *root_dir, const StringList *source_paths, StringList *libs) {
    size_t i;

    for (i = 0; i < source_paths->len; ++i) {
        char *absolute = path_join(root_dir, source_paths->items[i]);
        char *content = read_text_file(absolute, NULL);

        free(absolute);
        if (content != NULL) {
            char *sanitized = strip_comments_and_literals(content);
            StringList includes;
            StringList system_includes;
            size_t rule_index;

            string_list_init(&includes);
            string_list_init(&system_includes);
            collect_includes(content, &includes, &system_includes);
            for (rule_index = 0; rule_index < system_includes.len; ++rule_index) {
                maybe_add_pkgconfig_from_include(system_includes.items[rule_index], libs);
            }
            for (rule_index = 0; rule_index < MAZEN_ARRAY_LEN(AUTO_LIB_RULES); ++rule_index) {
                if (rule_matches(&AUTO_LIB_RULES[rule_index], &includes, sanitized, false)) {
                    add_rule_libs(&AUTO_LIB_RULES[rule_index], libs);
                }
            }
            string_list_free(&system_includes);
            string_list_free(&includes);
            free(sanitized);
            free(content);
        }
    }
}

bool autolib_infer_from_linker_output(const char *output, StringList *libs) {
    StringList empty_includes;
    size_t before = libs->len;
    size_t i;

    string_list_init(&empty_includes);
    for (i = 0; i < MAZEN_ARRAY_LEN(AUTO_LIB_RULES); ++i) {
        if (rule_matches(&AUTO_LIB_RULES[i], &empty_includes, output, true)) {
            add_rule_libs(&AUTO_LIB_RULES[i], libs);
        }
    }
    string_list_free(&empty_includes);
    return libs->len > before;
}
