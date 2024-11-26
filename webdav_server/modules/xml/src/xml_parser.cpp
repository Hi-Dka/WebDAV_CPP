#include "xml_parser.h"
#include <sstream>
#include <stack>

namespace webdav {

XMLParser::XMLParser() {}

XMLParser::~XMLParser() {}

bool XMLParser::parse(const std::string& xml, std::shared_ptr<XMLNode>& root) {
    size_t pos = 0;
    root = std::make_shared<XMLNode>();
    return parse_node(xml, pos, root);
}

bool XMLParser::parse_node(const std::string& xml, size_t& pos, std::shared_ptr<XMLNode>& node) {
    // 跳过空白字符
    while (pos < xml.length() && std::isspace(xml[pos])) pos++;
    
    if (pos >= xml.length() || xml[pos] != '<') return false;
    pos++; // 跳过 '<'
    
    // 获取标签名
    node->name = get_tag_name(xml, pos);
    if (node->name.empty()) return false;
    
    // 解析属性
    if (!parse_attributes(xml, pos, node->attributes)) return false;
    
    // 检查是否是自闭合标签
    while (pos < xml.length() && std::isspace(xml[pos])) pos++;
    if (pos < xml.length() && xml[pos] == '/') {
        pos += 2; // 跳过 "/>"
        return true;
    }
    
    if (pos >= xml.length() || xml[pos] != '>') return false;
    pos++; // 跳过 '>'
    
    // 解析子节点和文本内容
    std::string content;
    while (pos < xml.length()) {
        while (pos < xml.length() && std::isspace(xml[pos])) pos++;
        if (pos >= xml.length()) break;
        
        if (xml[pos] == '<') {
            if (xml[pos + 1] == '/') { // 结束标签
                pos += 2;
                std::string end_tag = get_tag_name(xml, pos);
                if (end_tag != node->name) return false;
                
                while (pos < xml.length() && xml[pos] != '>') pos++;
                if (pos >= xml.length()) return false;
                pos++;
                
                if (!content.empty()) {
                    node->value = content;
                }
                return true;
            } else { // 子节点
                auto child = std::make_shared<XMLNode>();
                child->parent = node;
                if (!parse_node(xml, pos, child)) return false;
                node->children.push_back(child);
            }
        } else {
            content += xml[pos++];
        }
    }
    
    return false;
}

bool XMLParser::parse_attributes(const std::string& xml, size_t& pos, 
                               std::map<std::string, std::string>& attributes) {
    while (pos < xml.length()) {
        while (pos < xml.length() && std::isspace(xml[pos])) pos++;
        if (pos >= xml.length()) return false;
        
        if (xml[pos] == '>' || xml[pos] == '/') return true;
        
        std::string name;
        while (pos < xml.length() && !std::isspace(xml[pos]) && xml[pos] != '=') {
            name += xml[pos++];
        }
        
        while (pos < xml.length() && std::isspace(xml[pos])) pos++;
        if (pos >= xml.length() || xml[pos] != '=') return false;
        pos++;
        
        while (pos < xml.length() && std::isspace(xml[pos])) pos++;
        if (pos >= xml.length() || xml[pos] != '"') return false;
        pos++;
        
        std::string value;
        while (pos < xml.length() && xml[pos] != '"') {
            value += xml[pos++];
        }
        
        if (pos >= xml.length()) return false;
        pos++; // 跳过结束引号
        
        attributes[name] = value;
    }
    
    return false;
}

std::string XMLParser::get_tag_name(const std::string& xml, size_t& pos) {
    std::string name;
    while (pos < xml.length() && !std::isspace(xml[pos]) && 
           xml[pos] != '>' && xml[pos] != '/') {
        name += xml[pos++];
    }
    return name;
}

std::string XMLParser::build(const std::shared_ptr<XMLNode>& root) {
    std::stringstream ss;
    
    ss << "<" << root->name;
    
    for (const auto& attr : root->attributes) {
        ss << " " << attr.first << "=\"" << attr.second << "\"";
    }
    
    if (root->children.empty() && root->value.empty()) {
        ss << "/>";
        return ss.str();
    }
    
    ss << ">";
    
    if (!root->value.empty()) {
        ss << root->value;
    }
    
    for (const auto& child : root->children) {
        ss << build(child);
    }
    
    ss << "</" << root->name << ">";
    return ss.str();
}

} // namespace webdav 