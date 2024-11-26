#ifndef XML_PARSER_H
#define XML_PARSER_H

#include "xml_types.h"

namespace webdav {

class XMLParser {
public:
    XMLParser();
    ~XMLParser();

    bool parse(const std::string& xml, std::shared_ptr<XMLNode>& root);
    std::string build(const std::shared_ptr<XMLNode>& root);

private:
    bool parse_node(const std::string& xml, size_t& pos, std::shared_ptr<XMLNode>& node);
    bool parse_attributes(const std::string& xml, size_t& pos, std::map<std::string, std::string>& attributes);
    std::string get_tag_name(const std::string& xml, size_t& pos);
};

} // namespace webdav

#endif // XML_PARSER_H 