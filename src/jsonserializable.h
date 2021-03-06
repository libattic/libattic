#ifndef JSONSERIALIZABLE_H_
#define JSONSERIALIZABLE_H_
#pragma once

#include <string>
#include <iostream>
#include <json/json.h>

namespace attic { 

class JsonSerializable {
public:
    JsonSerializable() {};
    virtual ~JsonSerializable(){};

    virtual void Serialize(Json::Value& root) = 0;
    virtual void Deserialize(Json::Value& root) = 0;
};

namespace jsn {
    static void PrintOutJsonValue(Json::Value* val);

    // TODO :: const correctness for input variables, will also need to mark serialize member
    // functions as const, save this for a slow day
    static bool SerializeObject(JsonSerializable* pObj, std::string& output);
    static bool SerializeObject(JsonSerializable* pObj, Json::Value &val);
    static bool SerializeJsonValue(Json::Value& root, std::string& output);
    static void SerializeVectorIntoObjectValue(Json::Value &val, std::vector<std::string> &vec);
    static void SerializeVector(const std::vector<std::string> &vec, Json::Value &val);
    static void SerializeMapIntoObject(Json::Value &val, std::map<std::string, std::string> &m);
    static void SerializeMapIntoObject(Json::Value &val, std::map<std::string, bool> &m);
    static void SerializeMapIntoObject(Json::Value &val, std::map<std::string, Json::Value> &m);
 
    // TODO :: fix all of these, inputs on the left outputs on the right
    static bool DeserializeJson(const std::string& input, Json::Value& val);
    static bool DeserializeJsonValue(Json::Value& val, const std::string& input); // depricated
    static bool DeserializeObject(JsonSerializable* pObj, const std::string& input);
    static bool DeserializeObject(JsonSerializable* pObj, Json::Value& val);
    static void DeserializeIntoVector(Json::Value &val, std::vector<std::string> &vec);
    static void DeserializeObjectValueIntoVector(Json::Value &val, std::vector<std::string> &vec);
    static void DeserializeObjectValueIntoMap(Json::Value &val, std::map<std::string, std::string> &m);
    static void DeserializeObjectValueIntoMap(Json::Value &val, std::map<std::string, bool> &m);
    static void DeserializeObjectValueIntoMap(Json::Value &val, std::map<std::string, Json::Value> &m);

    static void PrintOutJsonValue(Json::Value* val);

    static void PrintOutSerializeableObject(JsonSerializable* pObj) {
        Json::Value val;
        if(SerializeObject(pObj, val))
            PrintOutJsonValue(&val);
        else 
            std::cout<<" failed to serialize " << std::endl;
    }

    static std::string DebugSerializeableObject(JsonSerializable* pObj) {
        std::string output;
        Json::Value val(Json::nullValue);
        if(SerializeObject(pObj, val)) { 
            std::string output;
            Json::StyledWriter writer;
            output = writer.write(val);
        }
        else  { 
            output = " failed to serialize\n";
        }
        return output;
    }



    static void PrintOutJsonValue(Json::Value* val) {
        if(val) {
            std::string output;
            Json::StyledWriter writer;
            output = writer.write(*val);
            std::cout<<"***********************************************\n" << std::endl;
            std::cout<<" json value writer : \n" << output << std::endl;
            std::cout<<"***********************************************\n" << std::endl;
        }
    }

    static bool SerializeObject(JsonSerializable* pObj, std::string& output) {
        if(!pObj)
            return false;

        Json::Value root(Json::nullValue);
        pObj->Serialize(root);

        Json::StyledWriter writer;
        output = writer.write(root);

        return true;
    }


    static bool SerializeObject(JsonSerializable* pObj, Json::Value &val) {
        if(pObj) {
            pObj->Serialize(val);
            return true;
        }
        return false;
    }

    static bool SerializeJsonValue(Json::Value& root, std::string& output) {
        Json::StyledWriter writer;
        output = writer.write(root);
        return true;
    }

    static bool DeserializeJson(const std::string& input, Json::Value& val) {
        Json::Reader reader;
        if(!reader.parse(input, val))
            return false;
        return true;
    }

    static bool DeserializeJsonValue(Json::Value& val, const std::string& input) // Depricated
    {
        Json::Reader reader;
        if(!reader.parse(input, val))
            return false;
        return true;
    }

    static bool DeserializeObject(JsonSerializable* pObj, const std::string& input) {
        if(input.empty()) {
            std::cout<<" jsonDeserializeObject input empty ... " << std::endl;
            return false;
        }

        if(!pObj) { 
            std::cout<<" jsonDeserializeObjec tInvalid object " << std::endl;
            return false;
        }

        Json::Value root;
        Json::Reader reader;

        if(!reader.parse(input, root)) { 
            std::cout<< " jsonDeserializeObject failed to parse " << std::endl;
            return false;
        }

        pObj->Deserialize(root);
        return true;
    }

    static bool DeserializeObject(JsonSerializable* pObj, Json::Value& val) {
        if(pObj && val.isObject()) {
            pObj->Deserialize(val);
            return true;
        }

        return false;
    }

    static void SerializeVectorIntoObjectValue(Json::Value &val, std::vector<std::string> &vec) {
        if(val.isObject()) {
            std::vector<std::string>::iterator itr = vec.begin();                               
            for(; itr != vec.end(); itr++)                                                      
                val[*itr];
        }                                                                                       
    }                                                                                           
                                                                                                
    static void SerializeVector(const std::vector<std::string> &vec, Json::Value &val) {
        std::vector<std::string>::const_iterator itr = vec.begin();                                   
        for(; itr != vec.end(); itr++)                                                          
            val.append(*itr);                                                                   
    }                                                                                           

    static void DeserializeIntoVector(Json::Value &val, std::vector<std::string> &vec) {
        vec.clear();
        Json::ValueIterator itr = val.begin();
        for(; itr != val.end(); itr++) { 
            if((*itr).isConvertibleTo(Json::stringValue))
                vec.push_back((*itr).asString());
        }
    }

    static void DeserializeObjectValueIntoVector(Json::Value &val, std::vector<std::string> &vec) {
        if(val.isObject()) {
            vec.clear();
            Json::ValueIterator itr = val.begin();
                                                                                    
            for(; itr != val.end(); itr++) { 
                vec.push_back(itr.key().asString());
            }
        }
    }


    static void SerializeMapIntoObject(Json::Value &val, std::map<std::string, Json::Value> &m) {
        if(val.isObject()) {
            std::map<std::string, Json::Value>::iterator itr = m.begin();

            for(;itr != m.end(); itr++) {
                val[(*itr).first] = (*itr).second;
            }
        }
    }

    static void SerializeMapIntoObject(Json::Value &val, std::map<std::string, std::string> &m) {
        if(val.isObject()) {
            std::map<std::string, std::string>::iterator itr = m.begin();

            for(;itr != m.end(); itr++) {
                val[(*itr).first] = (*itr).second;
            }

        }
    }

    static void SerializeMapIntoObject(Json::Value &val, std::map<std::string, bool> &m) {
        if(val.isObject()) {
            std::map<std::string, bool>::iterator itr = m.begin();

            for(;itr != m.end(); itr++) {
                val[(*itr).first] = (*itr).second;
            }
        }
    }

    static void DeserializeObjectValueIntoMap(Json::Value &val, std::map<std::string, std::string> &m) {
        if(val.isObject()){
            m.clear();
            Json::ValueIterator itr = val.begin();

            for(; itr != val.end(); itr++) {
                if(itr.key().isString() && (*itr).isString())
                    m[itr.key().asString()] = (*itr).asString();
                else { 
                    if((*itr).type() == 6) {
                        Json::Value arr(Json::arrayValue);
                        arr = (*itr);

                        std::cout << "parsing array " << std::endl;
                        Json::ValueIterator itr2 = arr.begin();

                        std::string vallist;
                        for(; itr2 != arr.end(); itr2++) {
                            if(!vallist.empty())
                                vallist += ", ";

                            if((*itr2).isString())
                                vallist += (*itr2).asString();
                        }

                        if(itr.key().isString())
                            m[itr.key().asString()] = vallist;
                    }
                    std::cout<<" Nonstandard json response in DeserializeObjectValueIntoMap of type(s) : " << itr.key().type() << " " << (*itr).type() <<std::endl;
                }
            }
        }
    }

    static void DeserializeObjectValueIntoMap(Json::Value &val, std::map<std::string, bool> &m) {
        if(val.isObject()) {
            m.clear();
            Json::ValueIterator itr = val.begin();

            for(; itr != val.end(); itr++) {
                m[itr.key().asString()] = (*itr).asBool();
            }
        }
    }

    static void DeserializeObjectValueIntoMap(Json::Value &val, std::map<std::string, Json::Value> &m) {
        if(val.isObject()) {
            m.clear();
            Json::ValueIterator itr = val.begin();

            for(; itr != val.end(); itr++) {
                m[itr.key().asString()] = (*itr);
            }
        }
    }


}}//namespace
#endif

