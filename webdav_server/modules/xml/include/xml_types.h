#ifndef XML_TYPES_H
#define XML_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace webdav {

struct XMLNode {
    std::string name;
    std::string value;
    std::map<std::string, std::string> attributes;
    std::vector<std::shared_ptr<XMLNode>> children;
    std::weak_ptr<XMLNode> parent;
};

} // namespace webdav

#endif // XML_TYPES_H 