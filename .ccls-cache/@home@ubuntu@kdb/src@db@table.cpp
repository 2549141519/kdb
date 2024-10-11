#include "memtable.h"
#include "request.h"
#include "status.h"
#include "table.h"



namespace kdb {

kLRUCache klrucache_;

void TaskWorker::addTask(TableTask task) {
  task_vec_mtx_.lock();
  bg_task_vec_.push_back(task);
  task_vec_mtx_.unlock();
}

uint32_t g_thread_n = 0;
auto current_id = g_thread_n++;
void TaskWorker::Run() {
  isRunning_ = true;

  worker_ = std::thread([&]() { 
    auto thread_name = "memworker_" + std::to_string(current_id);
    auto ue = ::pthread_setname_np(::pthread_self(), thread_name.c_str());
    assert(ue != ERANGE);
    
    while(isRunning_) {
        if (bg_task_vec_.empty()) {
        std::unique_lock<std::mutex> lgk(mtx_);
        cond_.wait(lgk);
      }
    }

    {
        task_vec_mtx_.lock();
        std::swap(bg_task_vec_, task_vec_);
        task_vec_mtx_.unlock();
    }
    for (auto& task : task_vec_) {
    switch (task.index()) {
        case 0: {
            if (auto set_context = std::get_if<std::shared_ptr<SetContext>>(&task)) {
                assert(*set_context != nullptr);
                memtable_->Set(*set_context);
            }
            break;
        }
        case 1: {
            if (auto get_context = std::get_if<std::shared_ptr<GetContext>>(&task)) {
                assert(*get_context != nullptr);
                
                memtable_->Get(*get_context);
                if ((*get_context)->code.getCode() == StatusCode::kOk ||
                    (*get_context)->code.getCode() == StatusCode::kDelete) {
                    // 触发回调函数
                    if ((*get_context)->get_callback) {
                        (*get_context)->get_callback(*get_context);
                    }
                }
            

            //没找到就要先去lru找
          bool isErase = false;
          auto ue =
              klrucache_.get((*get_context)->key, &((*get_context)->value), &isErase);
          if (ue == true) {
            // 当前 kv 记录已经被删除
            if (isErase == true) {
              (*get_context)->code.setCode(StatusCode::kDelete);
              if ((*get_context)->get_callback) {
                (*get_context)->get_callback(*get_context);
              }
            } else {
             (*get_context)->code.setCode(StatusCode::kOk);
              if ((*get_context)->get_callback) {
                (*get_context)->get_callback(*get_context);
              }
            }
          }

          only_read_memtable->Get(*get_context);
                if ((*get_context)->code.getCode() == StatusCode::kOk ||
                    (*get_context)->code.getCode() == StatusCode::kDelete) {
                    // 触发回调函数
                    if ((*get_context)->get_callback) {
                        (*get_context)->get_callback(*get_context);
                    }
                }   

            // 只读 memtable 和 当前内存表中都没有找到
          // 在 memtable_view_vec 中找    
          if ((*get_context)->code.getCode() == StatusCode::kNotFound) {
            memview_manager_->getRequest(
                                         *get_context);
            // 直接触发回调函数.
            if ((*get_context)->get_callback) {
              (*get_context)->get_callback(*get_context);
            }
          }
          // 在 memtable_view_vec 中寻找
          // 这是一个很缓慢的过程
          // 将慢查询的kv数据放入到 klrucache 中去
          // 没有找到 和 被删除的 kv 都加入
          if ((*get_context)->code.getCode() == StatusCode::kNotFound ||
              (*get_context)->code.getCode() == StatusCode::kDelete) {
            // 结果不存在
            klrucache_.setEraseRecord((*get_context)->key);
          } else if ((*get_context)->code.getCode() == StatusCode::kOk) {
            klrucache_.set((*get_context)->key, (*get_context)->value);
          }
          break;
        }
        }
        case 2:{
          auto del_context = std::get<std::shared_ptr<DeleteContext>>(task);
          //
          assert(del_context != nullptr);
          // TODO
          // 当无法在 memtable 中找到需要被删除的
          memtable_->Delete(del_context);
          if (del_context->del_callback) {
            del_context->del_callback(del_context);
          }
        }
        default:
            break; // 需要加上一个 break
    }
}

   
   });
    
}



}