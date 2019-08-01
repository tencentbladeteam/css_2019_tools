// anglefuzz.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
#include "pch.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>

//http://learnwebgl.brown37.net/12_shader_language/documents/_GLSL_ES_Specification_1.0.17.pdf

enum define_proto_bytecount {
    PT_TYPE_IDENTIFIER = 0,  //对应一个类型
    PT_VARIANT_NUM     = 1,  //取出第二个字符，0～255，拼接成a0~a255
    PT_ADDITIONAL_SIZE = 2,  //如果大于0， 则给a初始化。一般有=和()两种形式，这个就不管了，直接原样读入后面的字符数。限制:32。
    PT_ADDITIONAL_DATA_START = 3,
};

//any define could go here.
std::string define_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 3)
        return "";

    remaining_size -= 3;

    int deftype = fuzzdata[PT_TYPE_IDENTIFIER];
    int variant_num = fuzzdata[PT_VARIANT_NUM];
    int additional_size = fuzzdata[PT_ADDITIONAL_SIZE];
    fuzzdata += 3;
    if (remaining_size < additional_size) {
        return "";
    }
    
    const char* presets[] = {"unsigned int", "signed int",
                             "bool", "int", "uint" , "float", "bvec2", "bvec3", "bvec4", "ivec2", "ivec3", "ivec4", "uvec2", "uvce3",
                             "uvec4", "vec2", "vec3", "vec4", "mat2" ,"mat3", "mat4", "mat2x3", "mat3x2", "mat2x4", "mat4x2", "mat3x4",
                             "mat4x3" };
    const char* type = presets[deftype % (sizeof(presets)/ sizeof(void*))];
    char tmp[60] = { 0 };
    snprintf(tmp, sizeof tmp, additional_size ? "%s a%d" : "%s a%d = ", type, variant_num);
    std::string out = tmp;
    if (additional_size) {
        char* additional_data = new char[additional_size + 1];
        memset(additional_data, 0, additional_size + 1);
        memcpy(additional_data, fuzzdata, additional_size);
        out.append(additional_data);
        delete[] additional_data;

        fuzzdata += additional_size;
        remaining_size -= additional_size;
    }

    //out += ";\n";  no we are writing a proto we dont need to add ; here.
    return out;
}


enum define_variant_op {
    VO_VARIANT_NUM = 0,  //0～255，拼接成a0~a255
    VO_OPERATE_TYPE = 1, //0: 数组, 1: .xxyy 类似这样的成员操作, 2: 一元操作, 3:直接读取fuzz数据（在左），4:直接读取fuzz（在右），5：不变
    VO_OPERATE_INDICATOR = 2, //如果是数组，则下标；如果是xxyy,则指定顺序；如果是一元操作，则代表正负、自增自减（在左或在右）、取反、取非；34则表明长度；5，直接原样返回
    VO_OPERATE_DATA = 3,
};
enum const_define_variant_op {
    CONST_VO_OPERATE_TYPE_MAX = 5,
    CONST_VO_OPERATE_INDICATOR_UNARY_MAX = 11,

};

std::string variant_op_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 3)
        return "";

    remaining_size -= 3;

    int variant_num = fuzzdata[VO_VARIANT_NUM];
    int op_type = fuzzdata[VO_OPERATE_TYPE] % (CONST_VO_OPERATE_TYPE_MAX + 1);
    int op_indicator = fuzzdata[VO_OPERATE_INDICATOR];

    const int buffer_size = 260;
    char* data_out = new char[buffer_size];
    memset(data_out, 0, buffer_size);
    
    switch (op_type) {
    case 0:
        //array
        snprintf(data_out, buffer_size, "a%d[%d]", variant_num, op_indicator); //dont add space, use a seperate space switch case in main loop.
        fuzzdata += 3;
        break;
    case 1:
        //member op
    {
        char selective[17] = { 'r', 'g', 'b', 'a', 's', 't', 'p', 'q', 'w', 'x', 'y', 'z', '\0' };
        if (remaining_size < 3){
            fuzzdata += 3;
            delete[] data_out;

            return "";
        }
        remaining_size -= 3;

        snprintf(data_out, buffer_size, "a%d.%c%c%c%c", variant_num,
            selective[op_indicator % sizeof(selective)],
            selective[fuzzdata[VO_OPERATE_INDICATOR + 1] % sizeof(selective)],
            selective[fuzzdata[VO_OPERATE_INDICATOR + 2] % sizeof(selective)],
            selective[fuzzdata[VO_OPERATE_INDICATOR + 3] % sizeof(selective)]);

        fuzzdata += 6;
    }
        break;
    case 2:
        //unary op
        switch(op_indicator % CONST_VO_OPERATE_INDICATOR_UNARY_MAX) {
        case 0:
            snprintf(data_out, buffer_size, "a%d++", variant_num);          break;
        case 1:
            snprintf(data_out, buffer_size, "a%d--", variant_num);          break;
        case 2:
            snprintf(data_out, buffer_size, "++a%d", variant_num);          break;
        case 3:
            snprintf(data_out, buffer_size, "--a%d", variant_num);          break;
        case 4:
            snprintf(data_out, buffer_size, "!a%d", variant_num);          break;
        case 5:
            snprintf(data_out, buffer_size, "~a%d", variant_num);          break;
        case 6:
            snprintf(data_out, buffer_size, "!!a%d", variant_num);          break;
        case 7:
            snprintf(data_out, buffer_size, "!~a%d", variant_num);          break;
        case 8:
            snprintf(data_out, buffer_size, "+a%d", variant_num);          break;
        case 9:
            snprintf(data_out, buffer_size, "-a%d", variant_num);          break;
        case 10:
            snprintf(data_out, buffer_size, "sizeof(a%d)", variant_num);          break;
        default:
            snprintf(data_out, buffer_size, "ERROR!!a%d", variant_num);    break; // shouldn't go here.
        }
        fuzzdata += 3;
        break;
    case 3:
        //left-reading fuzz data
        if (remaining_size < op_indicator){
            fuzzdata += 3;
            delete[] data_out;

            return "";
        }
        remaining_size -= op_indicator;
        memcpy(data_out, &fuzzdata[VO_OPERATE_DATA], op_indicator); //op indicator should be <= 255. we still have 5 bytes to use

        snprintf(&data_out[op_indicator], buffer_size - op_indicator, "a%d", variant_num);

        fuzzdata += 3;
        fuzzdata += op_indicator;
        break;
    case 4:
        //right-reading fuzz data
        if (remaining_size < op_indicator) {
            fuzzdata += 3;
            delete[] data_out;

            return "";
        }
        remaining_size -= op_indicator;
        
        snprintf(data_out, buffer_size, "a%d", variant_num);

        memcpy(&data_out[strlen(data_out)], 
               &fuzzdata[VO_OPERATE_DATA], op_indicator); 
        fuzzdata += 3;
        fuzzdata += op_indicator;
        break;
    case 5:
        //keep original
        snprintf(data_out, buffer_size, "a%d", variant_num);
        fuzzdata += 3;
        break;
    default:
        fuzzdata += 3;
        break;
    }

    std::string p = data_out;
    delete[] data_out;
    return p;
}

enum define_binary_operator_op {
    BI_OPERATE_INDICATOR = 0, 
};

std::string binary_operator_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";


    int variant_num = fuzzdata[BI_OPERATE_INDICATOR];
    fuzzdata++;
    remaining_size -= 1;

    const char* arr_list[] = { 
        "+", "-", "*", "/",  "%", " defined "
        "&", "^", "|", ",",
        ">", "<", "%", "\\", "="
        "||", "&&", ">>", "<<", "==", ">=", "<=", ">>=", "<<=", "%="
        ".", "+=", "-=", "*=", "/=", "&=", "^=", "|=", "?", ":",
        "//", "/*", "*/"
    };

    return arr_list[variant_num % (sizeof(arr_list) / sizeof(void*))];
}


enum define_numeric_operator_op {
    NU_SIZE_INDICATOR = 0,
    NU_HEX_OCT_DEC = 1,
};

std::string numeric_operator_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 2)
        return "";


    int num_size = fuzzdata[NU_SIZE_INDICATOR];
    int num_type = fuzzdata[NU_HEX_OCT_DEC];
    fuzzdata++;
    remaining_size -= 2;

    if (remaining_size < num_size)
        return "0";
    std::string out;

    switch (num_type % 6) {
    case 0:
        out = "-";  break;
    case 1:
        out = "0x"; break;
    case 2:
        out = "0"; break;
    case 3:
        out = "-0x"; break;
    case 4:
        out = "-0"; break;
    case 5: default:
        break;
    }

    //leonwxqian: yes i know there shouldn't be "." in hex and "a" in dec and "8" in oct here ..
    // just trying to mess the things up.
    const char* arr_list_hex[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "."  
        "a", "b", "c", "d", "e", "f"
    };

    const char* arr_list_dec[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "."
        "a"  
    };

    const char* arr_list_oct[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8" 
    };

    for (int i = 0; i < num_size; i++) {

        switch (num_type % 6) {
        case 1: case 3:
            out += arr_list_hex[fuzzdata[i] % (sizeof(arr_list_hex) / sizeof(void*))];
            break;
        case 2: case 4:
            out += arr_list_oct[fuzzdata[i] % (sizeof(arr_list_oct) / sizeof(void*))];
            break;
        case 0: case 5: default:
            out += arr_list_dec[fuzzdata[i] % (sizeof(arr_list_dec) / sizeof(void*))];

            break;
        }

    }

    num_type /= 10;
    switch (num_type % 6) {
    case 0:
        out += "e+";  break;
    case 1:
        out += "e-"; break;
    case 2:
        out += "u"; break;
    case 3:
        out += "l"; break;
    case 4:
        out += "f"; break;
    case 5: default:
        out += "."; break;
        break;
    }



    fuzzdata += num_size;
    remaining_size -= num_size;

    return out;
}

enum define_type_op {
    TY_INDICATOR = 0,
};

std::string get_typename_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    remaining_size -= 1;
    int type_indicator = fuzzdata[TY_INDICATOR];
    fuzzdata++;

    std::string out;

    const char* arr_list[] = { "signed", "unsigned",
                         "bool ", "int", "uint" , "float", "bvec2", "bvec3", "bvec4", "ivec2", "ivec3", "ivec4", "uvec2", "uvce3",
                         "uvec4", "vec2", "vec3", "vec4", "mat2" ,"mat3", "mat4", "mat2x3", "mat3x2", "mat2x4", "mat4x2", "mat3x4",
                         "mat4x3"
    };
    
    out += arr_list[type_indicator % (sizeof(arr_list) / sizeof(void*))];
    out += " ";

    return out;
}

enum define_builtin_func_op {
    BF_INDICATOR = 0,
};
std::string get_builtin_funcname_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    remaining_size -= 1;
    int type_indicator = fuzzdata[BF_INDICATOR];
    fuzzdata++;

    std::string out;

    const char* arr_list[] = { "abs", "acos", "acosh","asin","asinh","atan","atanh","ceil","clamp","cos","cosh","degrees","dFdx","dFdy","exp","exp2","faceforward",
"floor","fract","fwidth","intBitsToFloat","uintBitsToFloat","inversesqrt","log","log2","max","min","mix","mod","normalize","pow","radians","reflect",
"refract","round","roundEven","sign","sin","sinh","smoothstep","sqrt","step","tan","tanh","modf","trunc","distance","length","floatBitsToUint",
"packHalf2x16","packUnorm2x16","packSnorm2x16","isinf","isnan","any","not","equal","greaterThan","lessThan","greaterThanEqual","lessThanEqual","notEqual",
"inverse","matrixCompMult","outerProduct","transpose","cross","unpackHalf2x16","unpackUnorm2x16","unpackSnorm2x16"};

    out += arr_list[type_indicator % (sizeof(arr_list) / sizeof(void*))];
    out += " ";

    return out;
}

enum define_gen_func_op { //function use f0~255 as name
    GF_INDICATOR = 0,
    GF_INDICATOR2 = 1,

};

std::string get_gen_funcname_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 2)
        return "";

    remaining_size -= 2;
    int type_indicator1 = fuzzdata[GF_INDICATOR];
    int type_indicator2 = fuzzdata[GF_INDICATOR2];
    int type_indicator = type_indicator1 << 8 + type_indicator2;
    fuzzdata += 2;

    std::string out;

    const char* arr_list[] = { "abs", "acos", "acosh","asin","asinh","atan","atanh","ceil","clamp","cos","cosh","degrees","dFdx","dFdy","exp","exp2","faceforward",
"floor","fract","fwidth","intBitsToFloat","uintBitsToFloat","inversesqrt","log","log2","max","min","mix","mod","normalize","pow","radians","reflect",
"refract","round","roundEven","sign","sin","sinh","smoothstep","sqrt","step","tan","tanh","modf","trunc","distance","length","floatBitsToUint",
"packHalf2x16","packUnorm2x16","packSnorm2x16","isinf","isnan","any","not","equal","greaterThan","lessThan","greaterThanEqual","lessThanEqual","notEqual",
"inverse","matrixCompMult","outerProduct","transpose","cross","unpackHalf2x16","unpackUnorm2x16","unpackSnorm2x16",
    "iadd","isub","imad","imul","rcpx","idiv","udiv","imod","umod","ishr","ushr","rsqx","sqrt","len2","len3","len4","dist1","dist2","dist3","dist4","det2","det3",
"det4","imin","umin","imax","umax","exp2x","log2x","step","smooth","floatBitsToInt","floatBitsToUInt","intBitsToFloat","uintBitsToFloat","packSnorm2x16","packUnorm2x16",
"packHalf2x16","unpackSnorm2x16","unpackUnorm2x16","unpackHalf2x16","m4x4","m4x3","m3x4","m3x3","m3x2","call","callnz","loop","endloop","label","powx","isgn","iabs","nrm2",
"nrm3","nrm4","sincos","endrep","texcoord","texkill","discard","texld","texbem","texbeml","texreg2ar","texreg2gb","texm3x2pad","texm3x2tex","texm3x3pad",
"texm3x3tex","reserved0","texm3x3spec","texm3x3vspec","expp","logp","texreg2rgb","texdp3tex","texm3x2depth","texdp3","texm3x3","texdepth","cmp0","icmp","ucmp",
"select","extract","insert","dp2add","fwidth","texldd","texldl","texbias","texoffset","texoffsetbias","texlodoffset","texelfetch","texelfetchoffset","texgrad",
"texgradoffset","breakp","texsize","phase","comment","ps_1_0","ps_1_1","ps_1_2","ps_1_3","ps_1_4","ps_2_0","ps_2_x","ps_3_0","vs_1_0","vs_1_1","vs_2_0",
"vs_2_x","vs_2_sw","vs_3_0","vs_3_sw","atan2","sinh","tanh","acosh","trunc","floor","round","roundEven","ceil","exp2","log2","ineg","isnan","isinf","forward1",
"forward2","forward3","forward4","reflect1","reflect2","reflect3","reflect4","refract1","refract2","refract3","refract4",      
    };

    
    out += arr_list[type_indicator % (sizeof(arr_list) / sizeof(void*))];
    out += " ";

    return out;
}

enum text_insertion_op { //function use f0~255 as name
    TI_INDICATOR_OPS = 0, //多少操作
    TI_INDICATOR_DIRECTION = 1, //向前或向后
    TI_INDICATOR_OFFSET     = 2, //偏移多少字节 （同时保留偏移）
    TI_INDICATOR_SIZE      = 3, //插入多少字节
};

std::string textinsertion_proto(unsigned char*& fuzzdata, int& remaining_size, const char* current_output) {
    static int offset = 0;  /*维持一个全局计数器，默认从0开始*/
    std::string in = current_output;
    std::string out;

    if (remaining_size < 4)
        return "";
    int op_count = fuzzdata[TI_INDICATOR_OPS];
    int op_direction = fuzzdata[TI_INDICATOR_DIRECTION] % 2 ? 1 : -1;
    int op_offset = fuzzdata[TI_INDICATOR_OFFSET];
    int op_size = fuzzdata[TI_INDICATOR_OFFSET];
    fuzzdata += 4;
    remaining_size -= 4;

    if (remaining_size < op_size || op_count == 0)
        return "";
    
    int i = 0;

    do {
        offset += op_direction * op_offset;
        if (offset < 0)
            offset = 0;
        else if (offset > in.length())
            offset = in.length();

        char* data_to_insert = new char[op_size + 1];
        memset(data_to_insert, 0, op_size + 1);
        memcpy(data_to_insert, fuzzdata, op_size);

        out = in.substr(0, offset);
        out += data_to_insert;
        out += in.substr(offset);
        
        delete[]data_to_insert;
        in = out;

        fuzzdata += op_size;
        remaining_size -= op_size;

        if (remaining_size < 3)
            return out;

        op_direction = fuzzdata[TI_INDICATOR_DIRECTION] % 2 ? 1 : -1;
        op_offset = fuzzdata[TI_INDICATOR_OFFSET];
        op_size = fuzzdata[TI_INDICATOR_OFFSET];

        fuzzdata += 3;
        remaining_size -= 3;

        if (remaining_size < op_size)
            break;

        i++;
    } while (i < op_count);

    return out;

}

/*
enum switch_op { 
    //read switch what from input.
    //add cases from input too.
    //add processing data from input too.
    //this is not a primitive
};
enum for_op { 
    //this is not a primitive
};
enum while_op { 
    //this is not a primitive
};
*/

//用Prewords来替换之前的for/while/switch之类的，用于简化
enum prewords_available_op {
    PA_INDICATOR = 0,
};

std::string prewords_available_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int type_indicator = fuzzdata[PA_INDICATOR];
    fuzzdata += 1;    
    remaining_size -= 1;


    std::string out;

    const char* arr_list[] = {
        //already-have:
        "attribute","const","uniform","varying","break;","continue;","do{","for(","while(",
        "if(","else","in","out","inout","void","true","false","lowp","mediump","highp","precision","invariant"
        "discard","return","struct",

        "signed", "unsigned",
        "bool ", "int", "uint", "float", "bvec2", "bvec3", "bvec4", "ivec2", "ivec3", "ivec4", "uvec2", "uvce3",
        "uvec4", "vec2", "vec3", "vec4", "mat2", "mat3", "mat4", "mat2x3", "mat3x2", "mat2x4", "mat4x2", "mat3x4",
        "mat4x3"

        // yeah we might also want to try int(int) like this.
    };

    out += arr_list[type_indicator % ((sizeof(arr_list) / sizeof(void*)) / sizeof(void*))];
    out += " ";

    return out;
}

enum prewords_navailable_op {
    PN_INDICATOR = 0,
};

std::string prewords_navailable_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int type_indicator = fuzzdata[PN_INDICATOR];
    fuzzdata += 1;
    remaining_size -= 1;


    std::string out;

    const char* arr_list[] = {

        //preserved:
        "asm","class","union","enum","typedef","template","this","packed", "goto","switch","default","inline",
        "noinline","volatile","public","static","extern","external","interface","flat","sampler2D","samplerCube"


    };

    out += arr_list[type_indicator % (sizeof(arr_list) / sizeof(void*))];
    out += " ";

    return out;
}

enum pragma_op {
    PM_INDICATOR = 0,
    PM_INDICATOR2 = 1,  //optional
    PM_INDICATOR3_EXT_NAME = 2, //optional
};

std::string pragma_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int type_indicator = fuzzdata[PM_INDICATOR];
    fuzzdata++;
    remaining_size -= 1;

    std::string out;

    const char* arr_list[] = { "#", "#define", "#undef", "#if", "#ifdef", "#ifndef", "#else" ,"#elif", "#endif",
        "#error", "#pragma", "#extension", "#version", "#line"
    };

    out += arr_list[type_indicator % (sizeof(arr_list) / sizeof(void*))];

    if (out == "#pragma") {
        if (remaining_size < 1)
            return out;
        int type_indicator2 = fuzzdata[0]; //because the fuzzdata is already moved, so we can't do too much here.
        fuzzdata++;
        remaining_size -= 1;

        if (type_indicator2 % 2) 
            out += " optimize";
        else
            out += " debug";

        type_indicator2 /= 10;

        if (type_indicator2 % 2) 
            out += "(on)";
        else 
            out += "(off)";
    }
    else if (out == "#version") {
        if (remaining_size < 1)
            return out;
        int type_indicator2 = fuzzdata[0]; //0~255. 
        fuzzdata++;
        remaining_size -= 1;
        
        char sztmp[6] = {0};
        snprintf(sztmp, 6, " %3d0", type_indicator2); //version is a number grouped by times of 10
        out += sztmp;
    }
    else if (out == "#extension") {
        if (remaining_size < 1)
            return out;
        int type_indicator2 = fuzzdata[0];
        int exttype = type_indicator2 % 2; // 0~1
        type_indicator2 /= 10;
        int extbehaviour = type_indicator2 % 4; // 0~3
        fuzzdata++;
        remaining_size -= 1;

        if (exttype) {
            out += " all : ";
        }
        else {
            if (remaining_size < 1)
                return out;
            int extlen = fuzzdata[0];
            fuzzdata++;
            remaining_size--;
            
            if (remaining_size < extlen)
                return out;
            char* tmp = new char[extlen + 1];
            memset(tmp, 0, extlen + 1);
            memcpy(tmp, &fuzzdata[0], extlen);
            out += tmp;
            delete[] tmp;
            out += " : ";

            fuzzdata += extlen;
            remaining_size -= extlen;
        }

        switch (extbehaviour) {
        case 0:
            out += "require\n";
            break;
        case 1:
            out += "enable\n";
            break;
        case 2:
            out += "warn\n";
            break;
        case 3:
        default:
            out += "disable\n";
            break;
        }

    }
    out += " ";

    return out;
}

enum quote_insertion_op {
    //this is a primitive
    QI_INDICATOR = 0,
};

std::string quote_insertion_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int type_indicator = fuzzdata[QI_INDICATOR];
    fuzzdata += 1;
    remaining_size -= 1;


    std::string out;

    const char* arr_list[] = {
       ";", "{", "}", "(", ")", "[", "]", ",", "\n", "\r", "\t"
    };

    out += arr_list[type_indicator % (sizeof(arr_list) / sizeof(void*))];
    out += " ";

    return out;
}


enum predefined_op {
    PD_INDICATOR = 0,
};

std::string predefined_insertion_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int type_indicator = fuzzdata[PD_INDICATOR];
    fuzzdata += 1;
    remaining_size -= 1;


    std::string out;

    const char* arr_list[] = {
      "__LINE__", "__FILE__", "__VERSION__", "GL_ES", "gl_Position" , "gl_PointSize", "gl_FragColor", "gl_FragData", "gl_FragCoord"

    };

    out += arr_list[type_indicator % (sizeof(arr_list) / sizeof(void*))];
    out += " ";

    return out;
}


enum raw_data_op {
    RD_INDICATOR = 0,
};

std::string raw_data_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int rawdata_size = fuzzdata[RD_INDICATOR];
    fuzzdata += 1;
    remaining_size -= 1;

    if (remaining_size < rawdata_size)
        return "";

    std::string out;

    char* data = new char[rawdata_size + 1];
    memset(data, 0, rawdata_size + 1);
    memcpy(data, fuzzdata, rawdata_size);
    out += data;
    delete[] data;

    fuzzdata += rawdata_size;
    remaining_size -= rawdata_size;

    return out;
}



enum qualifier_op {
    QF_INDICATOR = 0,
};

std::string qualifier_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int type = fuzzdata[QF_INDICATOR];

    fuzzdata += 1;
    remaining_size -= 1;
    std::string out;
    
    switch (type % 7) {
    case 0:
        out = "layout(location ="; 
        out += numeric_operator_proto(fuzzdata, remaining_size);
        out += ") ";
        break;
    case 1:
        out = "layout(stream ="; 
        out += numeric_operator_proto(fuzzdata, remaining_size);
        out += ") ";
        break;
    case 2:
        out = "layout(std430, binding = "; 
        out += numeric_operator_proto(fuzzdata, remaining_size);
        out += ") ";
        break;
    case 3:
        out = "layout(xfb_buffer ="; 
        out += numeric_operator_proto(fuzzdata, remaining_size);
        out += ") ";
        break;
    case 4:
        out = "layout(xfb_buffer =";
        out += numeric_operator_proto(fuzzdata, remaining_size);
        out = ", xfb_stride ="; 
        out += numeric_operator_proto(fuzzdata, remaining_size);
        out += ") ";
        break;
    case 5:
        out = "layout(origin_upper_left) "; break;
    case 6:
    default:
        out = "layout(index =  ";
        out += numeric_operator_proto(fuzzdata, remaining_size);
        out += ") ";
        break;

    }

    return out;
}

enum struct_op {
    ST_INDICATOR = 0,
};

std::string gen_struct_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int member_count = fuzzdata[ST_INDICATOR];

    fuzzdata += 1;
    remaining_size -= 1;
    std::string out = "struct a";
    if (member_count > 127) {
        out = "union a";
    }

    out += std::to_string(member_count);
    out += "{\n";

    for(int i = 0; i < member_count; i++) {
        out += define_proto(fuzzdata, remaining_size);
        out += ";\n";
    }
    out += "};\n";
    return out;
}



enum arr_query_op {
    AR_INDICATOR = 0,
};

std::string gen_arrquery_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 1)
        return "";

    int arr_idx = fuzzdata[AR_INDICATOR];

    fuzzdata += 1;
    remaining_size -= 1;
    std::string out = "[";
    out += std::to_string(arr_idx);
    out += "]";

    return out;
}

enum function_define_op {
    FD_RET_TYPE = 0,  //返回值
    FD_FNAME    = 1,  //函数名
    FD_ARG_COUNT = 2, //参数数量
    /*
    FD_ARG1_TYPE = 3,
    FD_ARG2_TYPE = 4, 
    ...
    */
};

std::string function_define_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 3)
        return "";

    int func_ret_type = fuzzdata[FD_RET_TYPE];
    int func_name = fuzzdata[FD_FNAME];
    int arg_count = fuzzdata[FD_ARG_COUNT];
    fuzzdata += 3;
    remaining_size -= 3;


    std::string out;

    const char* type_list_func[] = { "void","bool", "int", "uint" , "float", "bvec2", "bvec3", "bvec4", "ivec2", "ivec3", "ivec4", "uvec2", "uvce3",
                         "uvec4", "vec2", "vec3", "vec4", "mat2" ,"mat3", "mat4", "mat2x3", "mat3x2", "mat2x4", "mat4x2", "mat3x4",
                         "mat4x3" };
    const char* type_list_arg[] = { "bool", "int", "uint" , "float", "bvec2", "bvec3", "bvec4", "ivec2", "ivec3", "ivec4", "uvec2", "uvce3",
                         "uvec4", "vec2", "vec3", "vec4", "mat2" ,"mat3", "mat4", "mat2x3", "mat3x2", "mat2x4", "mat4x2", "mat3x4",
                         "mat4x3" }; //remove bool.

    out += type_list_func[func_ret_type % (sizeof(type_list_func) / sizeof(void*))];
    out += " a";   //int aXX(
    out += std::to_string(func_name);
    out += "(";

    for (int i = 0; i < arg_count; i++) {
        if (remaining_size < 1)
            return out;
        int arg_ret_type = fuzzdata[0];

        out += type_list_arg[arg_ret_type % (sizeof(type_list_arg) / sizeof(void*))];
        out += " a";   //int_
        out += std::to_string(i);
        if (i != arg_count - 1)
            out += ",";

        remaining_size--;
        fuzzdata++;
    }

    out += "){\n";  //int aXX(int a0, bool b0, xxx c0){

    out += "return ";
    return out;  
}


enum for_while_define_op {
    FW_FOR_WHILE_OR_DO = 0,
    FW_START = 1,  //起始值
    FW_END_VALUE = 2,  //中止值
    FW_STEP_VALUE = 3, //步进
};
std::string loop_proto(unsigned char*& fuzzdata, int& remaining_size) {
    if (remaining_size < 4)
        return "";

    int for_while_do = fuzzdata[FW_FOR_WHILE_OR_DO];
    int startvalue = fuzzdata[FW_START];
    int endvalue = fuzzdata[FW_END_VALUE];
    int stepvalue = fuzzdata[FW_STEP_VALUE];
    fuzzdata += 4;
    remaining_size -= 4;


    std::string out;

    if (for_while_do % 3 == 0) { //for
        out = "for (int a255 = ";
        out += std::to_string(startvalue > 127 ? -1 * (startvalue - 127) : startvalue);
        out += "; a255 < ";
        out += std::to_string(endvalue > 127 ? -1 * (endvalue - 127) : endvalue);
        out += "; a255 += ";
        out += std::to_string(stepvalue > 127 ? -1 * (stepvalue - 127) : stepvalue);
        out += "){\n";
    }
    else if (for_while_do % 3 == 1) {
        out = "int a255(";
        out += std::to_string(startvalue > 127 ? -1 * (startvalue - 127) : startvalue);
        out += ";\nwhile (a255 <";
        out += std::to_string(endvalue > 127 ? -1 * (endvalue - 127) : endvalue);
        out += "){\n";
        out += "a255 += ";
        out += std::to_string(stepvalue > 127 ? -1 * (stepvalue - 127) : stepvalue);
        out += ";\n";
    }
    else if (for_while_do % 3 == 2) {
        out = "int a255(";
        out += std::to_string(startvalue > 127 ? -1 * (startvalue - 127) : startvalue);
        out += ";\ndo{";
        out += "a255 += ";
        out += std::to_string(stepvalue > 127 ? -1 * (stepvalue - 127) : stepvalue);
        out += ";\n}while (a255 <";
        out += std::to_string(endvalue > 127 ? -1 * (endvalue - 127) : endvalue);
        out += ");\n";
    }
    return out;

}


int main()
{
    unsigned char* data = (unsigned char*) "agn\x9e\x8o\x11noiwenyoiwnepymw";
    int len = strlen((const char*)data);
    std::string out;

    while (len) {
        //we want a weighted result, so don't use switch.. 

        unsigned char ch = data[0];
        data++;
        len--;

        if (ch >= 0 && ch < 30)
            out += quote_insertion_proto(data, len);
        else if (ch >= 30 && ch < 40)
            out += predefined_insertion_proto(data, len);
        else if (ch >= 40 && ch < 50)
            out += pragma_proto(data, len);
        else if (ch >= 50 && ch < 53)
            out += prewords_navailable_proto(data, len);
        else if (ch >= 53 && ch < 70)
            out += prewords_available_proto(data, len);
        else if (ch >= 70 && ch < 80)
            out += get_builtin_funcname_proto(data, len);
        else if (ch >= 80 && ch < 90)
            out = textinsertion_proto(data, len, out.c_str()); //yes assign.
        else if (ch >= 90 && ch < 100)
            out += get_gen_funcname_proto(data, len); //general func name
        else if (ch >= 100 && ch < 110)
            out += get_typename_proto(data, len);
        else if (ch >= 110 && ch < 120)
            out += numeric_operator_proto(data, len);
        else if (ch >= 120 && ch < 130)
            out += binary_operator_proto(data, len);
        else if (ch >= 130 && ch < 140)
            out += variant_op_proto(data, len);
        else if (ch >= 140 && ch < 150)
            out += define_proto(data, len);
        else if (ch >= 150 && ch < 160)
            out += qualifier_proto(data ,len);
        else if (ch >= 160 && ch < 170)
            out += gen_arrquery_proto(data, len);
        else if (ch >= 180 && ch < 190)
            out += gen_struct_proto(data, len);
        else if (ch >= 190 && ch < 200)
            out += function_define_proto(data, len);
        else
            out += raw_data_proto(data, len);
     
        out += " ";
    }

    int n = out.size();
    int m = out.length();
    std::cout << out;

    return 0;
}
