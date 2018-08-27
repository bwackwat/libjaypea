std::string hostname;
std::string admin;
std::string public_directory;
std::string ssl_certificate;
std::string ssl_private_key;
int http_port;
int https_port;
int chat_port;
int cache_megabytes = 30;
std::string connection_string;

Util::define_argument("hostname", hostname, {"-hn"});
Util::define_argument("admin", hostname, {"-a"});
Util::define_argument("public_directory", public_directory, {"-pd"});
Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
Util::define_argument("http_port", &http_port, {"-p"});
Util::define_argument("https_port", &https_port, {"-sp"});
Util::define_argument("chat_port", &chat_port, {"-cp"});
Util::define_argument("cache_megabytes", &cache_megabytes,{"-cm"});
Util::define_argument("postgresql_connection", connection_string, {"-pcs"});
Util::parse_arguments(argc, argv, "This is a server application for jph2.net.");

TlsEpollServer server(ssl_certificate, ssl_private_key, static_cast<uint16_t>(https_port), 10);
SymmetricEncryptor encryptor;
HttpApi api(public_directory, &server, &encryptor);
api.set_file_cache_size(cache_megabytes);
