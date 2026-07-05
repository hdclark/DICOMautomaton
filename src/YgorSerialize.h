#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <istream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace serialization {

template<typename T, typename = void>
struct class_version : std::integral_constant<unsigned int, 0> {};

#define BOOST_CLASS_VERSION(T, Version) \
    namespace boost { namespace serialization { \
    template<> struct class_version<T> : std::integral_constant<unsigned int, Version> {}; \
    } }

template<typename T>
struct nvp {
    const char *name = nullptr;
    T &value;
};

template<typename T>
auto make_nvp(const char *name, T &value) -> nvp<T> {
    return nvp<T>{name, value};
}

template<typename Archive, typename T>
void serialize(Archive &, T &, const unsigned int);

template<typename Archive, typename T, typename = void>
struct has_serialize : std::false_type {};

template<typename Archive, typename T>
struct has_serialize<Archive, T, std::void_t<decltype(boost::serialization::serialize(std::declval<Archive &>(), std::declval<T &>(), std::declval<unsigned int>()))>> : std::true_type {};

template<typename Archive, typename T>
auto call_custom_serialize(Archive &ar, T &value, const unsigned int version, std::true_type) -> void {
    using boost::serialization::serialize;
    serialize(ar, value, version);
}

template<typename Archive, typename T>
auto call_custom_serialize(Archive &, T &, const unsigned int, std::false_type) -> void {
}

}  // namespace serialization
}  // namespace boost

namespace boost {
namespace archive {

struct no_codecvt_t {};
inline constexpr no_codecvt_t no_codecvt{};

struct archive_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

namespace detail {

template<typename T>
using enable_if_string = std::enable_if_t<std::is_same_v<std::decay_t<T>, std::string>, int>;

template<typename T, typename = void>
struct is_string_like : std::false_type {};

template<typename T>
struct is_string_like<T, std::enable_if_t<std::is_same_v<std::decay_t<T>, std::string>>> : std::true_type {};

template<typename T, typename = void>
struct is_vector_like : std::false_type {};

template<typename T>
struct is_vector_like<std::vector<T>, void> : std::true_type {};

template<typename T, typename = void>
struct is_list_like : std::false_type {};

template<typename T>
struct is_list_like<std::list<T>, void> : std::true_type {};

template<typename T, typename = void>
struct is_map_like : std::false_type {};

template<typename T, typename U>
struct is_map_like<std::map<T, U>, void> : std::true_type {};

template<typename T, typename = void>
struct is_shared_ptr_like : std::false_type {};

template<typename T>
struct is_shared_ptr_like<std::shared_ptr<T>, void> : std::true_type {};

inline std::string read_token(std::istream &in) {
    std::string token;
    std::getline(in, token);
    if(!in) {
        throw archive_error("unexpected end of archive");
    }
    return token;
}

inline void write_token(std::ostream &out, const std::string &token) {
    out << token << '\n';
}

inline void write_string(std::ostream &out, const std::string &value) {
    out << "str:" << value.size() << '\n';
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
    out << '\n';
}

inline std::string read_string(std::istream &in) {
    const std::string token = read_token(in);
    if(token.rfind("str:", 0) != 0) {
        throw archive_error("expected string token");
    }
    const auto length = std::stoul(token.substr(4));
    std::string data(length, '\0');
    in.read(data.data(), static_cast<std::streamsize>(length));
    if(!in || static_cast<std::size_t>(in.gcount()) != length) {
        throw archive_error("unexpected end of string payload");
    }
    std::string trailing;
    std::getline(in, trailing);
    return data;
}

inline void skip_payload(std::istream &in, std::size_t length) {
    in.ignore(static_cast<std::streamsize>(length));
}

}  // namespace detail

class basic_archive {
  protected:
    std::ostream *out_ = nullptr;
    std::istream *in_ = nullptr;

  public:
    virtual ~basic_archive() = default;

    explicit basic_archive(std::ostream &out) : out_(&out) {}
    explicit basic_archive(std::istream &in) : in_(&in) {}

    template<typename T>
    void save(T &value) {
        if(!out_) {
            throw archive_error("archive is not writable");
        }
        serialize_value(*out_, value, 0);
    }

    template<typename T>
    void load(T &value) {
        if(!in_) {
            throw archive_error("archive is not readable");
        }
        serialize_value(*in_, value, 0);
    }

    template<typename T>
    auto operator&(T &value) -> basic_archive & {
        if(out_) {
            serialize_value(*out_, value, 0);
        } else {
            serialize_value(*in_, value, 0);
        }
        return *this;
    }

    template<typename T>
    auto operator&(boost::serialization::nvp<T> &value) -> basic_archive & {
        return (*this & value.value);
    }

    template<typename T>
    auto operator&(boost::serialization::nvp<T> &&value) -> basic_archive & {
        return (*this & value.value);
    }

    template<typename T>
    void serialize_value(std::ostream &out, T &value, const unsigned int version) {
        using std::decay_t;
        if constexpr(detail::is_string_like<T>::value) {
            detail::write_string(out, value);
        } else if constexpr(std::is_arithmetic_v<decay_t<T>> || std::is_enum_v<decay_t<T>>) {
            detail::write_token(out, std::string("scalar:") + std::to_string(value));
        } else if constexpr(detail::is_vector_like<decay_t<T>>::value) {
            detail::write_token(out, std::string("vector:") + std::to_string(value.size()));
            for(auto &entry : value) {
                serialize_value(out, entry, version);
            }
        } else if constexpr(detail::is_list_like<decay_t<T>>::value) {
            detail::write_token(out, std::string("list:") + std::to_string(value.size()));
            for(auto &entry : value) {
                serialize_value(out, entry, version);
            }
        } else if constexpr(detail::is_map_like<decay_t<T>>::value) {
            detail::write_token(out, std::string("map:") + std::to_string(value.size()));
            for(auto &entry : value) {
                serialize_value(out, const_cast<typename decay_t<T>::key_type &>(entry.first), version);
                serialize_value(out, entry.second, version);
            }
        } else if constexpr(detail::is_shared_ptr_like<decay_t<T>>::value) {
            if(value) {
                detail::write_token(out, "ptr:1");
                serialize_value(out, *value, version);
            } else {
                detail::write_token(out, "ptr:0");
            }
        } else {
            (void)version;
            throw archive_error("unsupported type for serialization");
        }
    }

    template<typename T>
    void serialize_value(std::istream &in, T &value, const unsigned int version) {
        using std::decay_t;
        if constexpr(detail::is_string_like<T>::value) {
            value = detail::read_string(in);
        } else if constexpr(std::is_arithmetic_v<decay_t<T>> || std::is_enum_v<decay_t<T>>) {
            std::string token = detail::read_token(in);
            if(token.rfind("scalar:", 0) != 0) {
                throw archive_error("expected scalar token");
            }
            std::stringstream ss(token.substr(7));
            ss >> value;
            if(!ss) {
                throw archive_error("failed to parse scalar token");
            }
        } else if constexpr(detail::is_vector_like<decay_t<T>>::value) {
            std::string token = detail::read_token(in);
            if(token.rfind("vector:", 0) != 0) {
                throw archive_error("expected vector token");
            }
            const auto size = std::stoul(token.substr(7));
            value.clear();
            value.reserve(size);
            for(std::size_t i = 0; i < size; ++i) {
                typename decay_t<T>::value_type entry{};
                serialize_value(in, entry, version);
                value.emplace_back(std::move(entry));
            }
        } else if constexpr(detail::is_list_like<decay_t<T>>::value) {
            std::string token = detail::read_token(in);
            if(token.rfind("list:", 0) != 0) {
                throw archive_error("expected list token");
            }
            const auto size = std::stoul(token.substr(5));
            value.clear();
            for(std::size_t i = 0; i < size; ++i) {
                typename decay_t<T>::value_type entry{};
                serialize_value(in, entry, version);
                value.emplace_back(std::move(entry));
            }
        } else if constexpr(detail::is_map_like<decay_t<T>>::value) {
            std::string token = detail::read_token(in);
            if(token.rfind("map:", 0) != 0) {
                throw archive_error("expected map token");
            }
            const auto size = std::stoul(token.substr(4));
            value.clear();
            for(std::size_t i = 0; i < size; ++i) {
                typename decay_t<T>::key_type key{};
                typename decay_t<T>::mapped_type mapped{};
                serialize_value(in, key, version);
                serialize_value(in, mapped, version);
                value.emplace(std::move(key), std::move(mapped));
            }
        } else if constexpr(detail::is_shared_ptr_like<decay_t<T>>::value) {
            std::string token = detail::read_token(in);
            if(token.rfind("ptr:", 0) != 0) {
                throw archive_error("expected pointer token");
            }
            if(token.substr(4) == "1") {
                value = std::make_shared<typename decay_t<T>::element_type>();
                serialize_value(in, *value, version);
            } else {
                value.reset();
            }
        } else {
            (void)version;
            throw archive_error("unsupported type for serialization");
        }
    }
};

class text_oarchive : public basic_archive {
  public:
    explicit text_oarchive(std::ostream &out, const no_codecvt_t & = no_codecvt) : basic_archive(out) {}
};

class text_iarchive : public basic_archive {
  public:
    explicit text_iarchive(std::istream &in, const no_codecvt_t & = no_codecvt) : basic_archive(in) {}
};

class binary_oarchive : public basic_archive {
  public:
    explicit binary_oarchive(std::ostream &out) : basic_archive(out) {}
};

class binary_iarchive : public basic_archive {
  public:
    explicit binary_iarchive(std::istream &in) : basic_archive(in) {}
};

class xml_oarchive : public basic_archive {
  public:
    explicit xml_oarchive(std::ostream &out, const no_codecvt_t & = no_codecvt) : basic_archive(out) {}
};

class xml_iarchive : public basic_archive {
  public:
    explicit xml_iarchive(std::istream &in, const no_codecvt_t & = no_codecvt) : basic_archive(in) {}
};

}  // namespace archive
}  // namespace boost
