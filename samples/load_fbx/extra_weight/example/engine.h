﻿//
// Created by captainchen on 2022/2/4.
//

#ifndef UNTITLED_ENGINE_H
#define UNTITLED_ENGINE_H

#include <iostream>
#include <fstream>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/string_cast_beauty.hpp>
#include "debug.h"

namespace Engine{
    //顶点
    struct Vertex{
        glm::vec3 position_;
        glm::vec4 color_;
        glm::vec2 uv_;
    };

    //Mesh文件头
    struct MeshFileHead{
        char type_[4];//文件类型文件头
        char name_[32];//名字
        unsigned short vertex_num_;//顶点个数
        unsigned short vertex_index_num_;//索引个数
    };

    //Mesh文件
    struct MeshFile{
        MeshFileHead head_;
        Vertex *vertex_;
        unsigned short *index_;

        MeshFile(){
            vertex_ = nullptr;
            index_ = nullptr;
        }

        ~MeshFile(){
            if(vertex_ != nullptr){
                delete [] vertex_;
                vertex_ = nullptr;
            }
            if(index_ != nullptr){
                delete [] index_;
                index_ = nullptr;
            }
        }

        // 写入文件
        void Write(const char* filePath){
            std::ofstream file(filePath, std::ios::binary);
            if(file.is_open()){
                file.write(reinterpret_cast<char*>(&head_), sizeof(head_));
                file.write(reinterpret_cast<char*>(vertex_), sizeof(Vertex) * head_.vertex_num_);
                file.write(reinterpret_cast<char*>(index_), sizeof(unsigned short) * head_.vertex_index_num_);
                file.close();
            }
        }
    };

    class Animation{
    public:
        std::string name_;
        unsigned short frame_count_;//帧数
        unsigned short frame_per_second_;//帧率

        // 存储骨骼名字 [bone_name , bone_name , ...]
        std::vector<std::string> bone_name_vec_;

        // 存储骨骼关键帧数据，外面数组下标表示第几帧，里面数组下表表示第几个Bone [["Bone.Matrix","Bone.001.Matrix"] , ["Bone.Matrix","Bone.001.Matrix"]]
        std::vector<std::vector<glm::mat4>> frame_bones_matrix_vec_;
    public:
        Animation(){
            frame_count_ = 0;
            frame_per_second_ = 0;
        }

        /// 获取骨骼index
        int get_bone_index(std::string bone_name){
            for (int i = 0; i < bone_name_vec_.size(); ++i) {
                if(bone_name_vec_[i]==bone_name){
                    return i;
                }
            }
            return -1;
        }

        // 写入文件
        void Write(std::string filePath){
            // 路径转小写
            std::transform(filePath.begin(), filePath.end(), filePath.begin(), ::tolower);
            // 替换文件名中的空格为_
            std::filesystem::path path(filePath);
            std::string file_name = path.filename().string();
            // 判断文件名中是否存在空格，替换为_
            if(file_name.find(" ") != std::string::npos){
                DEBUG_LOG_ERROR("Animation::Write filePath:%s contains blank,will replace with _", filePath.c_str());
                std::replace(file_name.begin(), file_name.end(), ' ', '_');
                path.replace_filename(file_name);
                filePath = path.string();
            }
            // 创建文件夹
            if (std::filesystem::exists(path)==false){
                DEBUG_LOG_INFO("{} not exist,will create.",filePath);
                if(path.has_filename()==false){
                    DEBUG_LOG_ERROR("{} is not correct file path.",filePath);
                    assert(false);
                }
                auto dir_path = path.parent_path();
                if(std::filesystem::exists(dir_path)==false){
                    std::error_code error_cord;
                    if(std::filesystem::create_directories(dir_path,error_cord)==false){
                        DEBUG_LOG_ERROR("{} not exist,create failed.",filePath);
                        assert(false);
                    }
                }
            }
            // 写入文件
            std::ofstream file(filePath, std::ios::binary);
            if(file.is_open()==false){
                DEBUG_LOG_ERROR("file open failed:{}",filePath);
                assert(false);
                return;
            }
            // 写入文件头 skeleton_anim
            std::string type = "skeleton_anim";
            file.write(type.c_str(), type.size());
            // 写入名字长度
            unsigned short name_len = name_.size();
            file.write(reinterpret_cast<char*>(&name_len), sizeof(unsigned short));
            // 写入名字
            file.write(name_.c_str(), name_.size());
            // 写入帧数
            file.write(reinterpret_cast<char*>(&frame_count_), sizeof(frame_count_));
            // 写入帧率
            file.write(reinterpret_cast<char*>(&frame_per_second_), sizeof(frame_per_second_));
            // 写入骨骼数量
            unsigned short bone_num = bone_name_vec_.size();
            file.write(reinterpret_cast<char*>(&bone_num), sizeof(unsigned short));
            // 写入骨骼名字
            for (int i = 0; i < bone_name_vec_.size(); ++i) {
                // 写入长度
                unsigned short bone_name_len = bone_name_vec_[i].size();
                file.write(reinterpret_cast<char*>(&bone_name_len), sizeof(unsigned short));
                // 写入名字
                file.write(bone_name_vec_[i].c_str(), bone_name_vec_[i].size());
            }
            // 写入骨骼关键帧数据
            for (int frame_index = 0; frame_index < frame_bones_matrix_vec_.size(); ++frame_index) {
                for (int bone_index = 0; bone_index < frame_bones_matrix_vec_[frame_index].size(); ++bone_index) {
                    glm::mat4 bone_matrix = frame_bones_matrix_vec_[frame_index][bone_index];
                    file.write(reinterpret_cast<char*>(&bone_matrix), sizeof(glm::mat4));
                }
            }
            file.close();
        }
    };

    /// 顶点关联骨骼及权重,每个顶点最多可以关联4个骨骼。
    struct VertexRelateBoneInfo{
        char bone_index_[4];//骨骼索引，一般骨骼少于128个，用char就行。
        char bone_weight_[4];//骨骼权重，权重不会超过100，所以用char类型就可以。

        VertexRelateBoneInfo(){
            for (int i = 0; i < sizeof(bone_index_); ++i) {
                bone_index_[i]=-1;
            }
            for (int i = 0; i < sizeof(bone_weight_); ++i) {
                bone_weight_[i]=-1;
            }
        }

        void Push(char bone_index,char bone_weight){
            for (int i = 0; i < sizeof(bone_index_); ++i) {
                if(bone_index_[i]==-1){
                    bone_index_[i]=bone_index;
                    if(bone_weight_[i]==-1){
                        bone_weight_[i]=bone_weight;
                        return;
                    }
                    DEBUG_LOG_ERROR("bone_index_ bone_weight_ not sync");
                }
            }
            DEBUG_LOG_ERROR("too much bone");
        }
    };

    class WeightFile{
    public:
        WeightFile(int vertex_count){
            vertex_relate_bone_infos_=new VertexRelateBoneInfo[vertex_count];
            vertex_count_=vertex_count;
        }

        void Push(int vertex_index,char bone_index,float bone_weight){
            if(bone_weight>=0){
                vertex_relate_bone_infos_[vertex_index].Push(bone_index,bone_weight);
            }else{
                vertex_relate_bone_infos_[vertex_index].Push(bone_index,-1);
            }
        }

        // 写入文件
        void Write(std::string filePath){
            // 路径转小写
            std::transform(filePath.begin(), filePath.end(), filePath.begin(), ::tolower);
            // 替换文件名中的空格为_
            std::filesystem::path path(filePath);
            std::string file_name = path.filename().string();
            // 判断文件名中是否存在空格，替换为_
            if(file_name.find(" ") != std::string::npos){
                DEBUG_LOG_ERROR("Animation::Write filePath:%s contains blank,will replace with _", filePath.c_str());
                std::replace(file_name.begin(), file_name.end(), ' ', '_');
                path.replace_filename(file_name);
                filePath = path.string();
            }
            // 创建文件夹
            if (std::filesystem::exists(path)==false){
                DEBUG_LOG_INFO("{} not exist,will create.",filePath);
                if(path.has_filename()==false){
                    DEBUG_LOG_ERROR("{} is not correct file path.",filePath);
                    assert(false);
                }
                auto dir_path = path.parent_path();
                if(std::filesystem::exists(dir_path)==false){
                    std::error_code error_cord;
                    if(std::filesystem::create_directories(dir_path,error_cord)==false){
                        DEBUG_LOG_ERROR("{} not exist,create failed.",filePath);
                        assert(false);
                    }
                }
            }
            // 写入文件
            std::ofstream file(filePath, std::ios::binary);
            if(file.is_open()==false){
                DEBUG_LOG_ERROR("file open failed:{}",filePath);
                assert(false);
                return;
            }
            // 写入文件头 weight
            file.write("weight",sizeof("weight")-1);
            file.write(reinterpret_cast<char*>(vertex_relate_bone_infos_), sizeof(vertex_relate_bone_infos_) * vertex_count_);
            file.close();
        }

    private:
        VertexRelateBoneInfo* vertex_relate_bone_infos_;
        int vertex_count_;
    };
}


#endif //UNTITLED_ENGINE_H
