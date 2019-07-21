#include <uWS/uWS.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "spline.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;


double get_lane(double car_d) {
  if (0 < car_d && car_d < 4) {
    return 0;
  } else if (4 < car_d && car_d < 8 ) {
    return 1;
  } else if (8 < car_d && car_d < 12) {
    return 2;
  }
  return -1;
}

vector<double> is_car_ahead(vector<vector<double>> sensor_fusion, double car_s, double car_d, int prev_size) {
  vector<double> vals = {0, 0, 0};
  double min_s = 999999999;
  double car_lane = get_lane(car_d);
  for (int i = 0; i < sensor_fusion.size(); i ++) {
    double check_car_d = sensor_fusion[i][6];
    double check_car_lane = get_lane(check_car_d);

    if (car_lane == check_car_lane) {
      vals[0] = true;
      double vx = sensor_fusion[i][3];
      double vy = sensor_fusion[i][4];
      double check_car_s = sensor_fusion[i][5];

      double check_speed = sqrt(pow(vx, 2) + pow(vy, 2));
      check_car_s += ((double) prev_size * 0.02 * check_speed); // if using previous points, can project s values outwards in time

      if (((check_car_s > car_s) && ((check_car_s - car_s) < 30)) && check_car_s < min_s) {
        vals[1] = true;
        vals[2] = check_speed;
        min_s = check_car_s;
      }
    }
  }
  return vals;
}

vector<bool> is_car_left(vector<vector<double>> sensor_fusion, double car_s, double car_d, int prev_size) {
  vector<bool> vals = {false, false};
  double car_lane = get_lane(car_d);
  for (int i = 0; i < sensor_fusion.size(); i ++) {
    double check_car_d = sensor_fusion[i][6];
    double check_car_lane = get_lane(check_car_d);

    if (car_lane == check_car_lane + 1) {
      vals[0] = true;
      double vx = sensor_fusion[i][3];
      double vy = sensor_fusion[i][4];
      double check_car_s = sensor_fusion[i][5];

      double check_speed = sqrt(pow(vx, 2) + pow(vy, 2));
      check_car_s += ((double) prev_size * 0.02 * check_speed); // if using previous points, can project s values outwards in time

      if  (abs(check_car_s - car_s) < 30) {
        vals[1] = true;
      }
    }
  }
  return vals;
}

vector<bool> is_car_right(vector<vector<double>> sensor_fusion, double car_s, double car_d, int prev_size) {
  vector<bool> vals = {false, false};
  double car_lane = get_lane(car_d);
  for (int i = 0; i < sensor_fusion.size(); i ++) {
    double check_car_d = sensor_fusion[i][6];
    double check_car_lane = get_lane(check_car_d);

    if (car_lane == check_car_lane - 1) {
      vals[0] = true;
      double vx = sensor_fusion[i][3];
      double vy = sensor_fusion[i][4];
      double check_car_s = sensor_fusion[i][5];

      double check_speed = sqrt(pow(vx, 2) + pow(vy, 2));
      check_car_s += ((double) prev_size * 0.02 * check_speed); // if using previous points, can project s values outwards in time

      if  (abs(check_car_s - car_s) < 30) {
        vals[1] = true;
      }
    }
  }
  return vals;
}




int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in);

  string line;
  while (getline(in_map_, line)) {
    std::istringstream iss(line);
    double x;
    double y;
    float s;
    float d_x;
    float d_y;
    iss >> x;
    iss >> y;
    iss >> s;
    iss >> d_x;
    iss >> d_y;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }

  // start in lane 1;
  int lane = 1;

  // Have a reference velocity to target
  double ref_vel = 0; // mph

  bool changing_lanes = false;

  h.onMessage([&ref_vel, &map_waypoints_x,&map_waypoints_y,&map_waypoints_s,
               &map_waypoints_dx,&map_waypoints_dy,&lane,&changing_lanes]
              (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
               uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);

        string event = j[0].get<string>();

        if (event == "telemetry") {
          // j[1] is the data JSON object

          // Main car's localization Data
          double car_x = j[1]["x"];
          double car_y = j[1]["y"];
          double car_s = j[1]["s"];
          double car_d = j[1]["d"];
          double car_yaw = j[1]["yaw"];
          double car_speed = j[1]["speed"];

          // Previous path data given to the Planner
          auto previous_path_x = j[1]["previous_path_x"];
          auto previous_path_y = j[1]["previous_path_y"];
          // Previous path's end s and d values
          double end_path_s = j[1]["end_path_s"];
          double end_path_d = j[1]["end_path_d"];

          // Sensor Fusion Data, a list of all other cars on the same side
          //   of the road.
          auto sensor_fusion = j[1]["sensor_fusion"];

          int prev_size = previous_path_x.size();

          if (prev_size > 0) {
            car_s = end_path_s;
          }

          if ((3.2 < car_d && car_d < 4.8) || ( 7.2 < car_d && car_d < 8.8)) {
            changing_lanes = true;
          } else {
            changing_lanes = false;
          }

          vector<double> car_ahead = is_car_ahead(sensor_fusion, car_s, car_d, prev_size);
          vector<bool> car_left = is_car_left(sensor_fusion, car_s, car_d, prev_size);
          vector<bool> car_right = is_car_right(sensor_fusion, car_s, car_d, prev_size);

          double lane_changed = false;

          if (!changing_lanes && car_ahead[0] == 1) { // car ahead in same lane
            std::cout << "car ahead lane: " << lane << " ref_vel: " << ref_vel << " d: " << car_d;
            if (car_ahead[1] == 1) { // car is too close


              if (!car_left[0] && lane > 0 ) { //No car to the left
                std::cout << "no car to left";
                lane -= 1;
                lane_changed = true;
              } else if (!car_right[0] && lane < 2) { // No car to the right
                std::cout << "no car to right";
                lane += 1;
                lane_changed = true;
              } else if (car_left[0] && !car_left[1] && lane > 0 ){
                std::cout << "car to left not too close";
                lane -= 1;
                lane_changed = true;
              } else if (car_right[0] && !car_right[1] && lane < 2) {
                std::cout << "car to right not too close";
                lane += 1;
                lane_changed = true;
              }



              if (!lane_changed) {
                if (ref_vel > car_ahead[2]) {
                  ref_vel -= 0.224;
                  std::cout << "\nfollowing car ahead";
                } else {
                  ref_vel = car_ahead[2];
                }
              }


              std::cout << std::endl;
            } else {
              if (ref_vel < 49.5) {
                std::cout << "increasing speed - car ahead not too close\n";
                ref_vel += 0.224;
              }
            }



            std::cout << std::endl;
          } else if (ref_vel < 49.5) {
            std::cout << "increasing speed \n";
            ref_vel += 0.224;
          }


          // Create a list of widely spaced (x,y) waypoints, evenly spaced at 30m
          // Later we will interpolate these waypoints with a spline and fill it in with more points that control speed.

          vector<double> ptsx;
          vector<double> ptsy;

          // reference x,y yaw states
          // either we will reference starting point as where the car is or at the previous paths end point

          double ref_x = car_x;
          double ref_y = car_y;
          double ref_yaw = deg2rad(car_yaw);

          // if the previous size is almost empty, use the car as starting reference
          if (prev_size < 2) {
            // Use two points that make the path tangent to the car
            double prev_car_x = car_x - cos(car_yaw);
            double prev_car_y = car_y - sin(car_yaw);

            ptsx.push_back(prev_car_x);
            ptsx.push_back(car_x);

            ptsy.push_back(prev_car_y);
            ptsy.push_back(car_y);
          }
          // use the previous path's end point as starting reference
          else {

            // Redefine reference state as previous path end point
            ref_x = previous_path_x[prev_size - 1];
            ref_y = previous_path_y[prev_size - 1];

            double ref_x_prev = previous_path_x[prev_size - 2];
            double ref_y_prev = previous_path_y[prev_size - 2];
            ref_yaw = atan2(ref_y - ref_y_prev, ref_x - ref_x_prev);

            // Use two points that make the path tangent to the previous path's end point
            ptsx.push_back(ref_x_prev);
            ptsx.push_back(ref_x);

            ptsy.push_back(ref_y_prev);
            ptsy.push_back(ref_y);

          }

          // In Frenet add evenly 30m spaced points ahead of the starting reference
          vector<double> next_wp0;
          vector<double> next_wp1;
          vector<double> next_wp2;

          if (changing_lanes) {
            next_wp0 = getXY(car_s + 15, (2 + 4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            next_wp1 = getXY(car_s + 30, (2 + 4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            next_wp2 = getXY(car_s + 45, (2 + 4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          } else {
            next_wp0 = getXY(car_s + 30, (2 + 4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            next_wp1 = getXY(car_s + 60, (2 + 4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            next_wp2 = getXY(car_s + 90, (2 + 4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          }

          ptsx.push_back(next_wp0[0]);
          ptsx.push_back(next_wp1[0]);
          ptsx.push_back(next_wp2[0]);

          ptsy.push_back(next_wp0[1]);
          ptsy.push_back(next_wp1[1]);
          ptsy.push_back(next_wp2[1]);

          for (int i = 0; i < ptsx.size(); i++) {
            // shift car reference angle to 0 degrees
            double shift_x = ptsx[i] - ref_x;
            double shift_y = ptsy[i] - ref_y;

            ptsx[i] = (shift_x * cos(0 - ref_yaw) - shift_y * sin(0 - ref_yaw));
            ptsy[i] = (shift_x * sin(0 - ref_yaw) + shift_y * cos(0 - ref_yaw));
          }

          // Debuging code
          // std::cout << std::endl;
          // for (int i = 0; i < ptsx.size(); i ++) {
          //   std::cout << "ptsx[" << i << "]: " << ptsx[i] << " ";
          //   std::cout << "ptsy[" << i << "]: " << ptsy[i] << std::endl;
          // }

          // create a spline
          tk::spline s;

          // set (x,y) points to the spline
          s.set_points(ptsx, ptsy);

          // Define the actual (x,y) points we will use for the planner
          vector<double> next_x_vals;
          vector<double> next_y_vals;

          // Start with all the previous path points from last time
          for (int i = 0; i < previous_path_x.size(); i++) {
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);
          }

          // Calculate how to break up spline points so that we travel at our desired reference velocity
          double target_x = 30.0;
          double target_y = s(target_x);
          double target_dist = sqrt(pow(target_x, 2) + pow(target_y, 2));

          double x_add_on = 0;

          // Fill up the rest of our path planner after filling it with previous points, here we will always output 50 points
          for(int i = 1; i <= 50 - previous_path_x.size(); i++) {
            if (ref_vel < 49.5 && changing_lanes) {
              std::cout << "increasing speed - changing lanes - car ahead not too close\n";
              ref_vel += 0.224;
            }


            double N = (target_dist / (0.02 * ref_vel / 2.24 ));
            double x_point = x_add_on + target_x / N;
            double y_point = s(x_point);

            x_add_on = x_point;

            double x_ref = x_point;
            double y_ref = y_point;

            // rotate back to normal after rotating it earlier
            x_point = (x_ref * cos(ref_yaw) - y_ref * sin(ref_yaw));
            y_point = (x_ref * sin(ref_yaw) + y_ref * cos(ref_yaw));

            x_point += ref_x;
            y_point += ref_y;

            next_x_vals.push_back(x_point);
            next_y_vals.push_back(y_point);
          }

          json msgJson;
          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\","+ msgJson.dump()+"]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }

  h.run();
}
