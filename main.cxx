// modified from https://github.com/tdlib/td/blob/master/example/cpp/td_example.cpp
//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2020
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Simple single-threaded example of TDLib usage.
// Real world programs should use separate thread for the user input.
// Example includes user authentication, receiving updates, getting chat list and sending text messages.

// overloaded
namespace detail {
template <class... Fs>
struct overload;

template <class F>
struct overload<F> : public F {
  explicit overload(F f) : F(f) {
  }
};
template <class F, class... Fs>
struct overload<F, Fs...>
    : public overload<F>
    , overload<Fs...> {
  overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
  }
  using overload<F>::operator();
  using overload<Fs...>::operator();
};
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
  return detail::overload<F...>(f...);
}

namespace td_api = td::td_api;


void print_user_info(td::tl_object_ptr<td_api::user>& object) {
  std::cout << "id: " << object->id_ << std::endl;
  std::cout << "username: " << object->username_ << std::endl;
  std::cout << "First Name: " << object->first_name_ << std::endl;
  std::cout << "Last Name: " << object->last_name_ << std::endl;
  std::cout << "Phone Number: " << object->phone_number_ << std::endl;
}

class TdExample {
 public:
  TdExample() {
    td::Client::execute({0, td_api::make_object<td_api::setLogVerbosityLevel>(1)});
    client_ = std::make_unique<td::Client>();
  }
  void getMultipleUserInfo(std::shared_ptr<std::vector<int32_t>> ids) {
    getMultipleUserInfo(ids, 0);
  }

  void getMultipleUserInfo(std::shared_ptr<std::vector<int32_t>> ids, uint32_t idx) {
    if (idx >= ids->size()) { return; }
    getUserInfo((*ids)[idx], [=] () { getMultipleUserInfo(ids, idx + 1); });
  }

  void getUserInfo(int32_t user_id, std::function<void()> cont) {
    send_query(td_api::make_object<td_api::getUser>(user_id),
               [=](Object object) {
                 auto user = td::move_tl_object_as<td_api::user>(object);
                 print_user_info(user);
                 cont();
               });

  }
  void getMultipleUserDataWithOrWithoutContact(std::shared_ptr<std::vector<int32_t>> ids) {
    getMultipleUserDataWithOrWithoutContact(ids, 0);
  }
  
  void getMultipleUserDataWithOrWithoutContact(std::shared_ptr<std::vector<int32_t>> ids, uint32_t idx) {
    if (idx >= ids->size()) { return; }
    getUserDataWithOrWithoutContact((*ids)[idx], [=]() { getMultipleUserDataWithOrWithoutContact(ids, idx + 1); });
  }
  void getUserDataWithOrWithoutContact(int32_t user_id, std::function<void()> cont) {
    send_query(td_api::make_object<td_api::getUser>(user_id),
               [=](Object object) {
                 auto user_ori = td::move_tl_object_as<td_api::user>(object);
                 auto username_ori = user_ori->username_;
                 auto phone_ori = user_ori->phone_number_;
                 auto first_name_ori = user_ori->first_name_;
                 auto last_name_ori = user_ori->last_name_;
                 send_query(td_api::make_object<td_api::removeContacts>(std::vector<std::int32_t>{user_id}),
                            [=, &user_ori](Object object) {
                              
                              send_query(td_api::make_object<td_api::getUser>(user_id),
                                         [=, &user_ori](Object object) {
                                           auto user_removed_contact = td::move_tl_object_as<td_api::user>(object);
                                           std::cout << "with contact:" << std::endl;
                                           std::cout << "username: " << username_ori << std::endl;
                                           std::cout << "First Name: " << first_name_ori << std::endl;
                                           std::cout << "Last Name: " << last_name_ori << std::endl;
                                           std::cout << "Phone Number: " << phone_ori << std::endl;
                                           std::cout << "without contact:" << std::endl;
                                           print_user_info(user_removed_contact);
                                           
                                           td_api::object_ptr<td::td_api::contact> new_contact
                                             = td_api::make_object<td_api::contact>(phone_ori,
                                                                                    first_name_ori,
                                                                                    last_name_ori,
                                                                                    std::string{0}, //vcard
                                                                                    user_id
                                                                                    );
                                           auto add_contact = td_api::make_object<td_api::addContact>();
                                           add_contact->contact_ = std::move(new_contact);
                                           add_contact->share_phone_number_ = false;
                                           send_query(std::move(add_contact),
                                                      [=](Object object) { cont(); });
                                           
                                         });
                            });
                 
               });
  }
  
  void loop() {
    while (true) {
      if (need_restart_) {
        restart();
      } else if (!are_authorized_) {
        process_response(client_->receive(10));
      } else if (last_response_id < current_query_id_) {
        process_response(client_->receive(1));
      } else {
        std::cout << "Enter action [q] quit [u] check for updates and request results [c] show chats [m <id> <text>] "
                     "send message [me] show self [con] show contacts [coninfo] show contacts info w/ and w/o contacts "
                     "(WARNING: backup contact info using [con] before using [coninfo] to be on the safe side) [i <id>] get user info [l] logout: "
                  << std::endl;
        std::string line;
        std::getline(std::cin, line);
        std::istringstream ss(line);
        std::string action;
        if (!(ss >> action)) {
          continue;
        }
        if (action == "q") {
          return;
        }
        if (action == "u") {
          std::cout << "Checking for updates..." << std::endl;
          while (true) {
            auto response = client_->receive(0);
            if (response.object) {
              process_response(std::move(response));
            } else {
              break;
            }
          }
        } else if (action == "con") {
          std::cout << "Getting Contacts..." << std::endl;
          send_query(td_api::make_object<td_api::getContacts>(),
                     [this](Object object) {
                       std::cout << to_string(object) << std::endl;
                       auto contacts = td::move_tl_object_as<td_api::users>(object);
                       getMultipleUserInfo(std::make_shared<std::vector<int32_t>>(contacts->user_ids_));
                     });
        } else if (action == "coninfo") {
          std::cout << "Getting Contacts..." << std::endl;
          send_query(td_api::make_object<td_api::getContacts>(),
                     [this](Object object) {
                       std::cout << to_string(object) << std::endl;
                       auto contacts = td::move_tl_object_as<td_api::users>(object);
                       getMultipleUserDataWithOrWithoutContact(std::make_shared<std::vector<int32_t>>(contacts->user_ids_));
                     });

        } /*else if (action == "test") {
          std::cout << " ---  test --- " << std::endl;
          std::string id_str;
          std::cout << "Id: ";
          std::getline(std::cin, id_str);
          std::istringstream id_ss(id_str);
          int32_t user_id;
          id_ss >> user_id;
          getUserDataWithOrWithoutContact(user_id, [](){});
                                  
        } */else if (action == "close") {
          std::cout << "Closing..." << std::endl;
          send_query(td_api::make_object<td_api::close>(), {});
        } else if (action == "me") {
          send_query(td_api::make_object<td_api::getMe>(),
                     [this](Object object) { std::cout << to_string(object) << std::endl; });
        } else if (action == "l") {
          std::cout << "Logging out..." << std::endl;
          send_query(td_api::make_object<td_api::logOut>(), {});
        } else if (action == "i") {
          std::cout << "Getting user info..." << std::endl;
          auto user_id = std::int32_t {};
          ss >> user_id;
          ss.get();
          std::cout << get_user_name(user_id) << std::endl;
          send_query(td_api::make_object<td_api::getUser>(user_id),
                     [this](Object object) { std::cout << to_string(object) << std::endl; });
        } else if (action == "c") {
          std::cout << "Loading chat list..." << std::endl;
          send_query(td_api::make_object<td_api::getChats>(nullptr, std::numeric_limits<std::int64_t>::max(), 0, 1000),
                     [this](Object object) {
                       if (object->get_id() == td_api::error::ID) {
                         return;
                       }
                       auto chats = td::move_tl_object_as<td_api::chats>(object);
                       for (auto chat_id : chats->chat_ids_) {
                         std::cout << "[id:" << chat_id << "] [title:" << chat_title_[chat_id] << "]" << std::endl;
                       }
                     });
        }
      }
    }
  }

 private:
  using Object = td_api::object_ptr<td_api::Object>;
  std::unique_ptr<td::Client> client_;

  td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
  bool are_authorized_{false};
  bool need_restart_{false};
  std::uint64_t current_query_id_{0};
  std::uint64_t authentication_query_id_{0};
  std::uint64_t last_response_id{0};

  std::map<std::uint64_t, std::function<void(Object)>> handlers_;

  std::map<std::int32_t, td_api::object_ptr<td_api::user>> users_;

  std::map<std::int64_t, std::string> chat_title_;
  

  void restart() {
    client_.reset();
    *this = TdExample();
  }

  void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
    auto query_id = next_query_id();
    if (handler) {
      handlers_.emplace(query_id, std::move(handler));
    }
    client_->send({query_id, std::move(f)});
  }

  void process_response(td::Client::Response response) {
    if (!response.object) {
      return;
    }
    //std::cout << "response id: " << response.id << std::endl;
    //std::cout << response.id << " " << to_string(response.object) << std::endl;
    if (response.id == 0) {
      return process_update(std::move(response.object));
    }
    if (response.id > last_response_id) {
      last_response_id = response.id;
    }
    auto it = handlers_.find(response.id);
    if (it != handlers_.end()) {
      it->second(std::move(response.object));
      handlers_.erase(response.id);
    }
  }

  std::string get_user_name(std::int32_t user_id) {
    auto it = users_.find(user_id);
    if (it == users_.end()) {
      return "unknown user";
    }
    return it->second->first_name_ + " " + it->second->last_name_;
  }

  void process_update(td_api::object_ptr<td_api::Object> update) {
    td_api::downcast_call(
        *update, overloaded(
                     [this](td_api::updateAuthorizationState &update_authorization_state) {
                       authorization_state_ = std::move(update_authorization_state.authorization_state_);
                       on_authorization_state_update();
                     },
                     [this](td_api::updateNewChat &update_new_chat) {
                       chat_title_[update_new_chat.chat_->id_] = update_new_chat.chat_->title_;
                     },
                     [this](td_api::updateChatTitle &update_chat_title) {
                       chat_title_[update_chat_title.chat_id_] = update_chat_title.title_;
                     },
                     [this](td_api::updateUser &update_user) {
                       auto user_id = update_user.user_->id_;
                       users_[user_id] = std::move(update_user.user_);
                     },/*
                     [this](td_api::updateNewMessage &update_new_message) {
                       auto chat_id = update_new_message.message_->chat_id_;
                       auto sender_user_name = get_user_name(update_new_message.message_->sender_user_id_);
                       std::string text;
                       if (update_new_message.message_->content_->get_id() == td_api::messageText::ID) {
                         text = static_cast<td_api::messageText &>(*update_new_message.message_->content_).text_->text_;
                       }
                       std::cout << "Got message: [chat_id:" << chat_id << "] [from:" << sender_user_name << "] ["
                                 << text << "]" << std::endl;
                                 },*/
                     [](auto &update) {}));
  }

  auto create_authentication_query_handler() {
    return [this, id = authentication_query_id_](Object object) {
      if (id == authentication_query_id_) {
        check_authentication_error(std::move(object));
      }
    };
  }

  void on_authorization_state_update() {
    authentication_query_id_++;
    td_api::downcast_call(
        *authorization_state_,
        overloaded(
            [this](td_api::authorizationStateReady &) {
              are_authorized_ = true;
              std::cout << "Got authorization" << std::endl;
            },
            [this](td_api::authorizationStateLoggingOut &) {
              are_authorized_ = false;
              std::cout << "Logging out" << std::endl;
            },
            [this](td_api::authorizationStateClosing &) { std::cout << "Closing" << std::endl; },
            [this](td_api::authorizationStateClosed &) {
              are_authorized_ = false;
              need_restart_ = true;
              std::cout << "Terminated" << std::endl;
            },
            [this](td_api::authorizationStateWaitCode &) {
              std::cout << "Enter authentication code: " << std::flush;
              std::string code;
              std::cin >> code;
              send_query(td_api::make_object<td_api::checkAuthenticationCode>(code),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitRegistration &) {
              std::string first_name;
              std::string last_name;
              std::cout << "Enter your first name: " << std::flush;
              std::cin >> first_name;
              std::cout << "Enter your last name: " << std::flush;
              std::cin >> last_name;
              send_query(td_api::make_object<td_api::registerUser>(first_name, last_name),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitPassword &) {
              std::cout << "Enter authentication password: " << std::flush;
              std::string password;
              std::cin >> password;
              send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitOtherDeviceConfirmation &state) {
              std::cout << "Confirm this login link on another device: " << state.link_ << std::endl;
            },
            [this](td_api::authorizationStateWaitPhoneNumber &) {
              std::cout << "Enter phone number: " << std::flush;
              std::string phone_number;
              std::cin >> phone_number;
              send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitEncryptionKey &) {
              std::cout << "Enter encryption key (for telegram session) or DESTROY (DESTROY destroys local session data): " << std::flush;
              std::string key;
              std::getline(std::cin, key);
              if (key == "DESTROY") {
                send_query(td_api::make_object<td_api::destroy>(), create_authentication_query_handler());
              } else {
                send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(std::move(key)),
                           create_authentication_query_handler());
              }
            },
            [this](td_api::authorizationStateWaitTdlibParameters &) {
              auto parameters = td_api::make_object<td_api::tdlibParameters>();
              parameters->database_directory_ = "tdlib";
              parameters->use_message_database_ = true;
              parameters->use_secret_chats_ = true;
              parameters->api_id_ = 94575;
              parameters->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
              parameters->system_language_code_ = "en";
              parameters->device_model_ = "Desktop";
              parameters->system_version_ = "Unknown";
              parameters->application_version_ = "1.0";
              parameters->enable_storage_optimizer_ = true;
              send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)),
                         create_authentication_query_handler());
            }));
  }

  void check_authentication_error(Object object) {
    if (object->get_id() == td_api::error::ID) {
      auto error = td::move_tl_object_as<td_api::error>(object);
      std::cout << "Error: " << to_string(error) << std::flush;
      on_authorization_state_update();
    }
  }

  std::uint64_t next_query_id() {
    return ++current_query_id_;
  }
};

int main() {
  TdExample example;
  example.loop();
}
