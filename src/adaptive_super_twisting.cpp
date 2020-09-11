#include <iostream>
#include <math.h>

#include "ros/ros.h"
#include "geometry_msgs/Pose2D.h"
#include "geometry_msgs/Vector3.h"
#include "std_msgs/Float64.h"
#include "std_msgs/UInt8.h"


class AdaptiveSuperTwistingControl
{
public:
  //Thruster outputs
  float starboard_t;
  float port_t;

  //Sensor feedback
  float theta;
  float u;
  float v;
  float r;

  static const float integral_step = 0.01;

  int testing;
  int arduino;

  //Tracking variables
  float u_d;
  float psi_d;

  //Auxiliry variables
  float e_u_int;
  float e_u_last;
  float psi_d_last;

  //Model pysical parameters
  float Xu;
  float Nr;
  static const float X_u_dot = -2.25;
  static const float Y_v_dot = -23.13;
  static const float N_r_dot = -2.79;
  float Xuu;
  static const float m = 30;
  static const float Iz = 4.1;
  static const float B = 0.41;
  static const float c = 0.78;
  
  float Tx;
  float Tz;
  float Ka_u;
  float Ka_psi;
  float Ka_dot_u;
  float Ka_dot_psi;
  float Ka_dot_last_u;
  float Ka_dot_last_psi;
  float ua_u;
  float ua_psi;
  float k2_u;
  float k2_psi;
  float x2_u;
  float x2_psi;
  float x2_dot_u;
  float x2_dot_psi;
  float x2_dot_last_u;
  float x2_dot_last_psi;

  //Controller gains
  float k_u;
  float k_psi;
  float kmin_u;
  float kmin_psi;
  float epsilon_u;
  float epsilon_psi;
  float miu_u;
  float miu_psi;
  float lambda_u;
  float lambda_psi;

  AdaptiveSuperTwistingControl()
  {
    //ROS Publishers for each required sensor data
    right_thruster_pub = n.advertise<std_msgs::Float64>("/usv_control/controller/right_thruster", 1000);
    left_thruster_pub = n.advertise<std_msgs::Float64>("/usv_control/controller/left_thruster", 1000);
    speed_gain_pub = n.advertise<std_msgs::Float64>("/usv_control/adaptive_super_twisting/speed_gain", 1000);
    speed_error_pub = n.advertise<std_msgs::Float64>("/usv_control/controller/speed_error", 1000);
    speed_sigma_pub = n.advertise<std_msgs::Float64>("/usv_control/adaptive_super_twisting/speed_sigma", 1000);
    heading_sigma_pub = n.advertise<std_msgs::Float64>("/usv_control/adaptive_super_twisting/heading_sigma", 1000);
    heading_gain_pub = n.advertise<std_msgs::Float64>("/usv_control/adaptive_super_twisting/heading_gain", 1000);
    heading_error_pub = n.advertise<std_msgs::Float64>("/usv_control/controller/heading_error", 1000);
    
    //ROS Subscribers
    desired_speed_sub = n.subscribe("/guidance/desired_speed", 1000, &AdaptiveSuperTwistingControl::desiredSpeedCallback, this);
    desired_heading_sub = n.subscribe("/guidance/desired_heading", 1000, &AdaptiveSuperTwistingControl::desiredHeadingCallback, this);
    ins_pose_sub = n.subscribe("/vectornav/ins_2d/ins_pose", 1000, &AdaptiveSuperTwistingControl::insCallback, this);
    local_vel_sub = n.subscribe("/vectornav/ins_2d/local_vel", 1000, &AdaptiveSuperTwistingControl::velocityCallback, this);
    flag_sub = n.subscribe("/arduino_br/ardumotors/flag", 1000, &AdaptiveSuperTwistingControl::flagCallback, this);
    ardu_sub = n.subscribe("arduino", 1000, &AdaptiveSuperTwistingControl::arduinoCallback, this);

    static const float dk_u = 0.4;
    static const float dk_psi = 0.5;
    static const float dkmin_u = 0.2;
    static const float dkmin_psi = 0.5;
    static const float depsilon_u = 0.3;
    static const float depsilon_psi = 0.05;
    static const float dmiu_u = 0.05;
    static const float dmiu_psi = 0.05;
    static const float dlambda_u = 0.001;
    static const float dlambda_psi = 1;

    n.param("/adaptive_super_twisting/k_u", k_u, dk_u);
    n.param("/adaptive_super_twisting/k_psi", k_psi, dk_psi);
    n.param("/adaptive_super_twisting/kmin_u", kmin_u, dkmin_u);
    n.param("/adaptive_super_twisting/kmin_psi", kmin_psi, dkmin_psi);
    n.param("/adaptive_super_twisting/epsilon_u", epsilon_u, depsilon_u);
    n.param("/adaptive_super_twisting/epsilon_psi", epsilon_psi, depsilon_psi);
    n.param("/adaptive_super_twisting/mu_u", miu_u, dmiu_u);
    n.param("/adaptive_super_twisting/mu_psi", miu_psi, dmiu_psi);
    n.param("/adaptive_super_twisting/lambda_u", lambda_u, dlambda_u);
    n.param("/adaptive_super_twisting/lambda_psi", lambda_psi, dlambda_psi);

    u_d = 0;
    psi_d = 0;
    testing = 0;
    arduino = 0;

    x2_u = 0;
    x2_psi = 0;
    x2_dot_u = 0;
    x2_dot_psi = 0;
    x2_dot_last_u = 0;
    x2_dot_last_psi = 0;

  }

  void desiredSpeedCallback(const std_msgs::Float64::ConstPtr& _ud)
  {
    u_d = _ud -> data;
  }

  void desiredHeadingCallback(const std_msgs::Float64::ConstPtr& _psid)
  {
    psi_d = _psid -> data;
  }

  void insCallback(const geometry_msgs::Pose2D::ConstPtr& _ins)
  {
    theta = _ins -> theta;
  }

  void velocityCallback(const geometry_msgs::Vector3::ConstPtr& _vel)
  {
    u = _vel -> x;
    v = _vel -> y;
    r = _vel -> z;
  }

  void flagCallback(const std_msgs::UInt8::ConstPtr& _flag)
  {
    testing = _flag -> data;
  }

  void arduinoCallback(const std_msgs::UInt8::ConstPtr& _ardu)
  {
    arduino = _ardu -> data;
  }

  void control()
  {
    if (testing == 1 && arduino == 1){
      Xu = -25;
      Xuu = 0;
      float u_abs = std::abs(u);
      if (u_abs > 1.2){
        Xu = 64.55;
        Xuu = -70.92;
      }

      Nr = (-0.52)*pow(pow(u,2) + pow(v,2),0.5);

      float g_u = (1 / (m - X_u_dot));
      float g_psi = (1 / (Iz - N_r_dot));

      float f_u = (((m - Y_v_dot)*v*r + (Xuu*u_abs*u + Xu*u)) / (m - X_u_dot));
      float f_psi = (((-X_u_dot + Y_v_dot)*u*v + (Nr*r)) / (Iz - N_r_dot));

      float e_u = u_d - u;
      float e_psi = psi_d - theta;
      if (std::abs(e_psi) > 3.141592){
          e_psi = (e_psi/std::abs(e_psi))*(std::abs(e_psi) - 2*3.141592);
      }
      e_u_int = (integral_step)*(e_u + e_u_last)/2 + e_u_int; //integral of the surge speed error
      e_u_last = e_u;

      float r_d = (psi_d - psi_d_last) / integral_step;
      psi_d_last = psi_d;

      float e_psi_dot = r_d - r;

      //float e_psi_dot = 0 - r;

      float sigma_u = e_u + lambda_u * e_u_int;
      float sigma_psi = e_psi_dot + lambda_psi * e_psi;
      
      float sigma_u_abs = std::abs(sigma_u);
      float sigma_psi_abs = std::abs(sigma_psi);
      
      int sign_u_sm = 0;
      int sign_psi_sm = 0;

      if (Ka_u > kmin_u){
          float signvar = sigma_u_abs - miu_u;
          if (signvar == 0){
            sign_u_sm = 0;
          }
          else {
            sign_u_sm = copysign(1,signvar);
          }
          Ka_dot_u = k_u * sign_u_sm;
      }
      else{
        Ka_dot_u = kmin_u;
      } 

      if (Ka_psi > kmin_psi){
        float signvar = sigma_psi_abs - miu_psi;
        if (signvar == 0){
          sign_psi_sm = 0;
        }
        else {
          sign_psi_sm = copysign(1,signvar);
        }
        Ka_dot_psi = k_psi * sign_psi_sm;
      }
      else{
        Ka_dot_psi = kmin_psi;
      }

      Ka_u = (integral_step)*(Ka_dot_u + Ka_dot_last_u)/2 + Ka_u; //integral to get the speed adaptative gain
      Ka_dot_last_u = Ka_dot_u;

      Ka_psi = (integral_step)*(Ka_dot_psi + Ka_dot_last_psi)/2 + Ka_psi; //integral to get the heading adaptative gain
      Ka_dot_last_psi = Ka_dot_psi;

      int sign_u = 0;
      int sign_psi = 0;

      if (sigma_u == 0){
        sign_u = 0;
      }
      else {
        sign_u = copysign(1,sigma_u);
      }

      k2_u = 2*epsilon_u*Ka_u;
      x2_dot_u = -(k2_u/2) * sign_u;
      x2_u = (integral_step)*(x2_dot_u + x2_dot_last_u)/2 + x2_u; //integral for x2
      x2_dot_last_u = x2_dot_u;

      ua_u = ((-Ka_u) * pow(sigma_u_abs,0.5) * sign_u) + x2_u;

      if (sigma_psi == 0){
        sign_psi = 0;
      }
      else {
        sign_psi = copysign(1,sigma_psi);
      }

      k2_psi = 2*epsilon_psi*Ka_psi;
      x2_dot_psi = -(k2_psi/2) * sign_psi;
      x2_psi = (integral_step)*(x2_dot_psi + x2_dot_last_psi)/2 + x2_psi; //integral for x2
      x2_dot_last_psi = x2_dot_psi;

      ua_psi = ((-Ka_psi) * pow(sigma_psi_abs,0.5) * sign_psi) + x2_psi;

      Tx = ((lambda_u * e_u) - f_u - ua_u) / g_u; //surge force
      Tz = ((lambda_psi * e_psi_dot) - f_psi - ua_psi) / g_psi; //yaw rate moment
      
      if (Tx > 73){
        Tx = 73;
      }
      else if (Tx < -60){
        Tx = -60;
      }
      if (Tz > 14){
        Tz = 14;
      }
      else if (Tz < -14){
        Tz = -14;
      }
      
      if (u_d == 0){
        Tx = 0;
        Tz = 0;
        Ka_u = 0;
        Ka_dot_last_u = 0;
        Ka_psi = 0;
        Ka_dot_last_psi = 0;
        x2_u = 0;
        x2_psi = 0;
        x2_dot_last_u = 0;
        x2_dot_last_psi = 0;
        e_u_int = 0;
        e_u_last = 0;
      }

      port_t = (Tx / 2) + (Tz / B);
      starboard_t = (Tx / (2*c)) - (Tz / (B*c));

      if (starboard_t > 36.5){
        starboard_t = 36.5;
      }
      else if (starboard_t < -30){
        starboard_t = -30;
      }
      if (port_t > 36.5){
        port_t = 36.5;
      }
      else if (port_t < -30){
        port_t = -30;
      }

      //Data publishing
      std_msgs::Float64 rt;
      std_msgs::Float64 lt;
      
      std_msgs::Float64 sg;
      std_msgs::Float64 hg;

      std_msgs::Float64 eu;
      std_msgs::Float64 epsi;

      std_msgs::Float64 su;
      std_msgs::Float64 sp;

      rt.data = starboard_t;
      lt.data = port_t;
      
      sg.data = Ka_u;
      hg.data = Ka_psi;

      eu.data = e_u;
      epsi.data = e_psi;

      su.data = sigma_u;
      sp.data = sigma_psi;

      right_thruster_pub.publish(rt);
      left_thruster_pub.publish(lt);

      speed_gain_pub.publish(sg);
      speed_error_pub.publish(eu);
      speed_sigma_pub.publish(su);
      heading_gain_pub.publish(hg);
      heading_error_pub.publish(epsi);
      heading_sigma_pub.publish(sp);
    }
  }

private:
  ros::NodeHandle n;

  ros::Publisher right_thruster_pub;
  ros::Publisher left_thruster_pub;
  ros::Publisher speed_gain_pub;
  ros::Publisher speed_error_pub;
  ros::Publisher speed_sigma_pub;
  ros::Publisher heading_sigma_pub;
  ros::Publisher heading_gain_pub;
  ros::Publisher heading_error_pub;

  ros::Subscriber desired_speed_sub;
  ros::Subscriber desired_heading_sub;
  ros::Subscriber ins_pose_sub;
  ros::Subscriber local_vel_sub;
  ros::Subscriber flag_sub;
  ros::Subscriber ardu_sub;
};

// Main
int main(int argc, char *argv[])
{
  ros::init(argc, argv, "adaptive_super_twisting");
  AdaptiveSuperTwistingControl adaptiveSuperTwistingControl;
  int rate = 100;
  ros::Rate loop_rate(rate);

  while (ros::ok())
  {
    adaptiveSuperTwistingControl.control();
    ros::spinOnce();
    loop_rate.sleep();
  }
  return 0;
}