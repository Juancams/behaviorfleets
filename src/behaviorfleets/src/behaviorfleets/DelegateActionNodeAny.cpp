// Copyright 2023 Intelligent Robotics Lab
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>
#include <iostream>
#include <fstream>

#include "behaviorfleets/DelegateActionNodeAny.hpp"

#include "behaviortree_cpp/behavior_tree.h"
#include "ament_index_cpp/get_package_share_directory.hpp"

#include "behaviortree_cpp/utils/shared_library.h"
#include "rclcpp/rclcpp.hpp"

#include "bf_msgs/msg/mission_command.hpp"
#include "bf_msgs/msg/mission_status.hpp"


namespace BF
{

DelegateActionNodeAny::DelegateActionNodeAny(
  const std::string & xml_tag_name,
  const BT::NodeConfig & conf)
: BT::ActionNodeBase(xml_tag_name, conf)
{
  std::string pkgpath, xml_path;

  remote_id_ = "not_set";
  remote_tree_ = "not_set";
  config().blackboard->get("node", node_);
  config().blackboard->get("pkgpath", pkgpath);

  getInput("remote_tree", remote_tree_);
  getInput("mission_id", mission_id_);

  std::cout << "remote_tree: " << remote_tree_ << std::endl;
  std::cout << "mission_id: " << mission_id_ << std::endl;

  xml_path = pkgpath + remote_tree_;
  std::cout << "xml_path: " << xml_path << std::endl;
  std::ifstream file(xml_path);
  std::ostringstream contents_stream;
  contents_stream << file.rdbuf();
  remote_tree_ = contents_stream.str();

  mission_pub_ = node_->create_publisher<bf_msgs::msg::MissionCommand>(
    "/mission_command", 100);

  poll_sub_ = node_->create_subscription<bf_msgs::msg::MissionStatus>(
    "/mission_poll", rclcpp::SensorDataQoS(),
    std::bind(&DelegateActionNodeAny::mission_poll_callback, this, std::placeholders::_1));
}

void
DelegateActionNodeAny::remote_status_callback(bf_msgs::msg::MissionStatus::UniquePtr msg)
{
  std::cout << "remote status received" << std::endl;
  remote_status_ = std::move(msg);
}

void
DelegateActionNodeAny::mission_poll_callback(bf_msgs::msg::MissionStatus::UniquePtr msg)
{
  std::cout << "poll received" << std::endl;
  // ignore answers from other robots
  if(!remote_identified_) {
    poll_answ_ = std::move(msg);
    remote_id_ = poll_answ_->robot_id;
    std::cout << "remote identified: " << remote_id_ << std::endl;

    remote_sub_ = node_->create_subscription<bf_msgs::msg::MissionStatus>(
      "/" + remote_id_ + "/mission_status", rclcpp::SensorDataQoS(),
      std::bind(&DelegateActionNodeAny::remote_status_callback, this, std::placeholders::_1));

    mission_pub_ = node_->create_publisher<bf_msgs::msg::MissionCommand>(
      "/" + remote_id_ + "/mission_command", 100);

    bf_msgs::msg::MissionCommand mission_msg;
    mission_msg.robot_id = remote_id_;
    mission_msg.mission_tree = remote_tree_;
    mission_pub_->publish(mission_msg);
    std::cout << "tree publised" << std::endl;
    std::cout << "status in /" << remote_id_ + "/mission_status" << std::endl;

    remote_identified_ = true;
  }
}

BT::NodeStatus
DelegateActionNodeAny::tick()
{
  if(!remote_identified_){
    bf_msgs::msg::MissionCommand msg;
    msg.mission_id = mission_id_;
    mission_pub_->publish(msg);
    std::cout << "mission publised" << std::endl;
  } else {
    if(remote_status_ != nullptr) {
      int status = remote_status_->status;
      switch(status) {
        case RUNNING:
          std::cout << "remote status: RUNNING" << std::endl;
          return BT::NodeStatus::RUNNING;
          break;
        case SUCCESS:
          std::cout << "remote status: SUCCESS" << std::endl;
          return BT::NodeStatus::SUCCESS;
          break;
        case FAILURE:
          std::cout << "remote status: FAILURE" << std::endl;
          return BT::NodeStatus::FAILURE;
          break;
        }
    }
  }

  return BT::NodeStatus::RUNNING;
}

}  // namespace BF

#include "behaviortree_cpp/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<BF::DelegateActionNodeAny>("DelegateActionNodeAny");
}
