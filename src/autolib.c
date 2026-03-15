#include "autolib.h"

#include <ctype.h>
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
     MAZEN_ARRAY_LEN(X11_IDENTIFIERS), NULL, 0}
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

static void collect_includes(const char *content, StringList *includes) {
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
            char terminator = *cursor == '"' ? '"' : '>';
            String include;

            string_init(&include);
            ++cursor;
            while (*cursor != '\0' && *cursor != terminator && *cursor != '\n') {
                string_append_char(&include, *cursor++);
            }
            if (*cursor == terminator) {
                string_list_push_unique_take(includes, string_take(&include));
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
            size_t rule_index;

            string_list_init(&includes);
            collect_includes(content, &includes);
            for (rule_index = 0; rule_index < MAZEN_ARRAY_LEN(AUTO_LIB_RULES); ++rule_index) {
                if (rule_matches(&AUTO_LIB_RULES[rule_index], &includes, sanitized, false)) {
                    add_rule_libs(&AUTO_LIB_RULES[rule_index], libs);
                }
            }
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
