#ifndef CONFIG_H_
#define CONFIG_H_

#include <libconfig.h++>
#include <string>

#include "log.h"

using namespace libconfig;

class MyConfigWrapper
{
private:
    Config* cfg;
public:
    MyConfigWrapper(string cfg_file_path)
    {
        cfg = new Config();
        try
        {
            cfg->readFile(cfg_file_path.c_str());
        }
        catch(const FileIOException &fioex)
        {
            panic("I/O error while reading libconfig cfg file");
        }
        catch(const ParseException &pex)
        {
            panic("Parse error while reading libconfig cfg file");
        }
    }
    string lookup(string key, string default_value)
    {
        try
        {
            string result = cfg->lookup(key);
            return result;
        }
        catch(const SettingNotFoundException &nfex)
        {
            return default_value;
        }
    }
};

#endif  // CONFIG_H_
