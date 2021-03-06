/**
 * @file Strategy.cpp
 *
 * @brief Localization challange pathplan strategy
 *
 * @date July 2017
 **/
/*****************************************************************************
** Includes
*****************************************************************************/
#include "Strategy.hpp"

Strategy::Strategy()
{
    _LocationState = turn;
    _Last_state = turn;
    _CurrentTarget = 0;
    _Location = new LocationStruct;
    _Env = new Environment;
    back_flag = false;
    cross_center_flag = false;
    stop_count = 0;
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 5; j++)
            through_path_ary[i][j] = 0;
    _Location->LocationPoint[0].y=0;
}
void Strategy::setParam(Parameter *Param)
{
    _Param = Param;
}
void Strategy::GameState(int int_GameState)
{
    switch (int_GameState)
    {
    case STATE_HALT:
        StrategyHalt();
        break;
    case STATE_LOCALIZATION:
        // StrategyLocalization();
        StrategyLocalization2();
        break;
    }
}
void Strategy::StrategyHalt()
{
    if(stop_count<5){
        _Env->Robot.v_x = 0;
        _Env->Robot.v_y = 0;
        _Env->Robot.v_yaw = 0;
        stop_count++;
    }//else{std::cout<<"stop end\n";}
    if(_Location->LocationPoint[0].x!=0||_Location->LocationPoint[0].y!=0){
        std::vector<int> order = OptimatePath();
    }
}
void Strategy::StrategyLocalization2()
{
    stop_count=0;
    RobotData Robot;
    Robot.pos.x = _Env->Robot.pos.x;
    Robot.pos.y = _Env->Robot.pos.y;
    // printf("x=%lf\ty=%lf\n",_Env->Robot.pos.x,_Env->Robot.pos.y);
    double imu = _Env->Robot.pos.angle;
    double absolute_front = imu + 90;
    static int flag = TRUE;
    static int flag_chase = TRUE;
    double ball_distance = 0.25;
    Robot.pos.x += ball_distance * cos(absolute_front * DEG2RAD);
    Robot.pos.y += ball_distance * sin(absolute_front * DEG2RAD);
    double lost_ball_dis = _Param->Strategy.HoldBall_Condition[3];
    double lost_ball_angle = _Param->Strategy.HoldBall_Condition[2];
    double hold_ball_dis = _Param->Strategy.HoldBall_Condition[1];
    double hold_ball_angle = _Param->Strategy.HoldBall_Condition[0];
    static double Begin_time = ros::Time::now().toSec(); // init timer begin
    double Current_time = ros::Time::now().toSec();      // init timer end
    double v_x, v_y, v_yaw;
    double accelerate = 1;
    double slow_factor = 1;
    static int IMU_state = 0;
    double compensation_distance = 0.05;
    double compensation_angle = ((int)absolute_front + 180) % 360;
    double compensation_x = compensation_distance * cos(compensation_angle * DEG2RAD);
    double compensation_y = compensation_distance * sin(compensation_angle * DEG2RAD);
    std::vector<int> order = OptimatePath();
    //===============TODO====================
    if (flag)
        Begin_time = Current_time;
    if (Current_time - Begin_time < accelerate)
        slow_factor = exp(-2.1 + 2 * ((Current_time - Begin_time) / accelerate));
    else
        slow_factor = 1;
    //=======================================
    //   ================================   Enable chase mode   ==================================
    bool chase_enable=false;
    if(_Param->Strategy.HoldBall_Condition.size()==5){
        chase_enable = _Param->Strategy.HoldBall_Condition[4];
        //std::cout<<"chase_enable_button_work"<<std::endl;
    }
    if(chase_enable){
        if(_Env->Robot.ball.distance > lost_ball_dis || fabs(_Env->Robot.ball.angle) > lost_ball_angle){
            _LocationState = chase;
             //_Param->NodeHandle.SPlanning_Velocity[3]=20;
        }else{

        }
    }
    //if(_Env->Robot.ball.distance > lost_ball_dis || fabs(_Env->Robot.ball.angle) > lost_ball_angle){
    //    if(flag_chase){
    //        _Last_state = _LocationState;
    //        _LocationState = chase;                      //  Check lost ball or not
    //        flag_chase = FALSE;
    //    }
    //}
    //else if(_Env->Robot.ball.distance < hold_ball_dis && fabs(_Env->Robot.ball.angle) < hold_ball_angle){
    //    _LocationState = _Last_state;
    //    flag_chase = TRUE;
    //}
    //   ================================   Enable chase mode end   ==================================
    Normalization(absolute_front);
    switch (_LocationState)
    {
    
    case forward: // Move to target point
        Forward(Robot, v_x, v_y, v_yaw, imu, flag, absolute_front, compensation_x, compensation_y);
        break;
    case chase:
        Chase(Robot, v_x, v_y, v_yaw);
        break;
    case turn:
        Turn(Robot, v_x, v_y, v_yaw, imu, flag, absolute_front);
        break;
    case finish:
        _LocationState = turn;
        _CurrentTarget = 0;
        back_flag=false;
        //_LocationState=restart;
        v_x = 0;
        v_y = 0;
        v_yaw = 0;
        _Env->GameState = STATE_HALT;   
        printf("Congratulation !!!\n");
        break;
    case restart:
        sleep(2);
        back_flag=false;
        _LocationState=turn;
        Turn(Robot, v_x, v_y, v_yaw, imu, flag, absolute_front);
        break;
    case error:
        printf("ERROR STATE\n");
        v_x = 0;
        v_y = 0;
        v_yaw = 0;
        //exit(FAULTEXECUTING);
        break;
    default: // ERROR SIGNAL
        printf("UNDEFINE STATE\n");
        //exit(FAULTEXECUTING);
    }
    showInfo(Robot.pos.x,Robot.pos.y,order, imu, compensation_x, compensation_y);
    Normalization(v_yaw);
    _Env->Robot.v_x = v_x;
    _Env->Robot.v_y = v_y;
    _Env->Robot.v_yaw = v_yaw;
}
void Strategy::Forward(RobotData &Robot, double &v_x, double &v_y, double &v_yaw, double imu, int &flag, double absolute_front, double compensation_x, double compensation_y)
{
    _Last_state = _LocationState;
    flag = FALSE;
    v_x = (_Target.TargetPoint[_CurrentTarget].x ) - Robot.pos.x;
    v_y = (_Target.TargetPoint[_CurrentTarget].y ) - Robot.pos.y;
    double v_x_temp, v_y_temp;
    //    <<<<<<<  HEAD   origin code in 2017.8.8
    // v_x_temp = v_x * cos((-imu) * DEG2RAD) - v_y * sin((-imu) * DEG2RAD);
    // v_y_temp = v_x * sin((-imu) * DEG2RAD) + v_y * cos((-imu) * DEG2RAD);
    //    >>>>>>>>  END   origin code in 2017.8.8

    //    <<<<<<<  HEAD   temp code in 2017.8.8
    double min_compensation = 0;                                            // because of yaw i need to rotate a min
    v_x_temp = v_x * cos((-imu + min_compensation) * DEG2RAD ) - v_y * sin((-imu + min_compensation) * DEG2RAD);       
    v_y_temp = v_x * sin((-imu + min_compensation) * DEG2RAD ) + v_y * cos((-imu + min_compensation) * DEG2RAD);
    //    >>>>>>>>  END   temp code in 2017.8.8
    v_x = v_x_temp;
    v_y = v_y_temp;
    v_yaw = atan2(_Target.TargetPoint[_CurrentTarget].y - Robot.pos.y, _Target.TargetPoint[_CurrentTarget].x - Robot.pos.x) * RAD2DEG - absolute_front;
    Normalization(v_yaw);
    //    <<<<<<<  HEAD  kym code in 2019.6.15
    //if(back_flag==true){
    //    if(v_yaw<0){
    //        v_yaw=180+v_yaw;
    //    }else{
    //        v_yaw=v_yaw-180;        
    //    }
    //}
    //    >>>>>>>>  END   kym code in 2017.6.15
    double center_circle_rangle=0.5;
    if(back_flag==true)center_circle_rangle=0.8;
    if(_Target.TargetPoint[_CurrentTarget].x==0&&_Target.TargetPoint[_CurrentTarget].y==0){
        //std::cout<<sqrt(Robot.pos.x*Robot.pos.x+Robot.pos.y*Robot.pos.y)<<"  "<<center_circle_rangle<<std::endl;
        if (sqrt(Robot.pos.x*Robot.pos.x+Robot.pos.y*Robot.pos.y) <= center_circle_rangle){
            //std::cout<<Robot.pos.x<<" "<<Robot.pos.y<<std::endl;
            if (_CurrentTarget == _Target.size){
                _LocationState = finish;
            }
            else
            {
                _LocationState = turn;
                
                _CurrentTarget++;
                cross_center_flag = false;
                //    <<<<<<<  HEAD  kym code in 2019.6.15
                double next_yaw = atan2(_Target.TargetPoint[_CurrentTarget].y - Robot.pos.y, _Target.TargetPoint[_CurrentTarget].x - Robot.pos.x) * RAD2DEG - absolute_front;
                Normalization(next_yaw);
                //std::cout<<next_yaw<<std::endl;
                //if(abs(next_yaw)>90){
                //    back_flag=true;    
                //}else{
                //    back_flag=false;                
                //}
                //    >>>>>>>>  END   kym code in 2017.6.15
                flag = TRUE;
               
            }
        }
    }
    if (fabs(v_x) <= 0.2 && fabs(v_y) <= 0.2)
    {
        if (_CurrentTarget == _Target.size)
            _LocationState = finish;
        else
        {
            _LocationState = turn;
            _CurrentTarget++;
            cross_center_flag = false;
            //    <<<<<<<  HEAD  kym code in 2019.6.15
            double next_yaw = atan2(_Target.TargetPoint[_CurrentTarget].y - Robot.pos.y, _Target.TargetPoint[_CurrentTarget].x - Robot.pos.x) * RAD2DEG - absolute_front;
            Normalization(next_yaw);
            //std::cout<<next_yaw<<std::endl;
            //if(abs(next_yaw)>90){
            //    back_flag=true;    
            //}else{
            //    back_flag=false;                
            //}
            //    >>>>>>>>  END   kym code in 2017.6.15
            flag = TRUE;            
        }
    }
    //========================
    //穿過中心偵測
    //y-y1=((y2-y1)/(x2-x1))*(x-x1)
    //m=(y2-y1)/(x2-x1)
    //m*x-m*x1=y-y1
    //m*x-m*x1-y+y1=0
    //(y2-y1)x-(x2-x1)y-(y2-y1)x1+(x2-x1)y1=0
    //ax+by+c=0
    //a=(y2-y1)
    //b=-(x2-x1)
    //c=-(y2-y1)x1+(x2-x1)y1
    //線外一點P到直線L距離為d(P,L)
    //d(P,L)=abs(ax0+by0+c)/sqrt(a*a+b*b)
    if (_Target.TargetPoint[_CurrentTarget].x==0&&_Target.TargetPoint[_CurrentTarget].y==0&&_CurrentTarget != _Target.size){
        double x1=Robot.pos.x;
        double y1=Robot.pos.y;
        double x2=_Target.TargetPoint[_CurrentTarget+1].x;
        double y2=_Target.TargetPoint[_CurrentTarget+1].y;
        double a=y2-y1;
        double b=-(x2-x1);
        double c=-(y2-y1)*x1+(x2-x1)*y1;
        double x0=0;
        double y0=0;
        double d_PL=fabs(a*x0+b*y0+c)/sqrt(a*a+b*b);
        if(d_PL<0.4){
            std::cout<<"cross center"<<std::endl;
            cross_center_flag = true;
            //std::cout<<a<<" "<<b<<" "<<c<<" "<<d_PL<<std::endl;
            _LocationState = turn;
            _CurrentTarget++;
        }
    }
    //========================
}
void Strategy::Turn(RobotData &Robot, double &v_x, double &v_y, double &v_yaw, double imu, int &flag, double absolute_front)
{
    static ros::Time last_time = ros::Time::now();
    ros::Time current_time = ros::Time::now();
    Vector3D vector_tr;
    vector_tr.x = _Target.TargetPoint[_CurrentTarget].x - Robot.pos.x;
    vector_tr.y = _Target.TargetPoint[_CurrentTarget].y - Robot.pos.y;
    vector_tr.yaw = atan2(vector_tr.y, vector_tr.x) * RAD2DEG - absolute_front;
    if (sqrt(vector_tr.x*vector_tr.x+vector_tr.y*vector_tr.y) < 0.7)
    {
        printf("FORCED Change mode to forward \n");
        _LocationState = forward;
    }
    if (fabs(vector_tr.x) < 0.2 && fabs(vector_tr.y) < 0.2)
    {
        //std::cout<<_CurrentTarget<<std::endl;
        _CurrentTarget++;
        cross_center_flag = false;
        //    <<<<<<<  HEAD  kym code in 2019.6.15
        double next_yaw = atan2(_Target.TargetPoint[_CurrentTarget].y - Robot.pos.y, _Target.TargetPoint[_CurrentTarget].x - Robot.pos.x) * RAD2DEG - absolute_front;
        Normalization(next_yaw);
        //std::cout<<next_yaw<<std::endl;
        //if(abs(next_yaw)>90){
        //    back_flag=true;    
        //}else{
        //    back_flag=false;                
        //}
        //    >>>>>>>>  END   kym code in 2017.6.15
        if (_CurrentTarget > _Target.size)
        {
            printf("error\n");
            _LocationState = finish;
        }
    }
    //    <<<<<<<  HEAD   origin code in 2017.8.8
    // v_y = 0.7;   // old value is 0.7 
    // v_x = 0;
    // v_yaw = vector_tr.yaw; // turn to target
    // Normalization(v_yaw);
    // if (fabs(v_yaw) <= 35)
    // {
    //     printf("Special slow motion \n");
    //     v_x = 0;
    //     v_y = 0;
    //     v_yaw = vector_tr.yaw; // turn to target
    //     if (fabs(v_yaw) <= 3)
    //     {
    //         printf("Change mode to forward \n");
    //         _LocationState = forward;
    //     }
    // }
    //    <<<<<<<  END    origin code in 2017.8.8

    //    <<<<<<<  HEAD   temp code in 2017.8.8
    // double v_strike = 0.01;
    v_y = 0.31;  
    v_x = 0;
    v_yaw = vector_tr.yaw; // turn to target
    //    <<<<<<<  HEAD  kym code in 2019.6.15
    Normalization(v_yaw);
    //if(back_flag==true){
    //    if(v_y>0){
    //        v_y=-v_y;
    //    }
    //    if(v_yaw<0){
    //        v_yaw=180+v_yaw;
    //    }else{
    //        v_yaw=v_yaw-180;        
    //    }
    //}
    //    >>>>>>>>  END   kym code in 2017.6.15
    
    if (fabs(v_yaw) <= 5)
    {
        printf("Change mode to forward \n");
        _LocationState = forward;
    }
    //    <<<<<<<  END    temp code in 2017.8.8
}
void Strategy::Chase(RobotData &,double &v_x, double &v_y, double &v_yaw)
{
    double ball_dis = _Env->Robot.ball.distance;
    double ball_ang = _Env->Robot.ball.angle;
    double lost_ball_dis = _Param->Strategy.HoldBall_Condition[3];
    double lost_ball_angle = _Param->Strategy.HoldBall_Condition[2];
    double hold_ball_dis = _Param->Strategy.HoldBall_Condition[1];
    double hold_ball_angle = _Param->Strategy.HoldBall_Condition[0];
    
    //rotate chase
    //v_x = _Env->Robot.ball.distance * cos(_Env->Robot.ball.angle * DEG2RAD) - _Env->Robot.ball.distance * sin(_Env->Robot.ball.angle * DEG2RAD);
    //v_y = _Env->Robot.ball.distance * sin(_Env->Robot.ball.angle * DEG2RAD) + _Env->Robot.ball.distance * cos(_Env->Robot.ball.angle * DEG2RAD);
    v_yaw = _Env->Robot.ball.angle;

    //straight chase
    v_x = -(_Env->Robot.ball.distance * sin((_Env->Robot.ball.angle) * DEG2RAD));
    v_y = _Env->Robot.ball.distance * cos((_Env->Robot.ball.angle) * DEG2RAD);
    v_yaw = _Env->Robot.ball.angle;
    

    if(ball_dis>0.5&&fabs(ball_ang)>20){
        v_y=v_y*0.5;
        v_x=v_x*0.5;
        v_yaw=v_yaw*1.3;
        if(v_yaw>180)v_yaw=180;
        if(v_yaw<-180)v_yaw=-180;
    }
    if(fabs(ball_ang)>20&&ball_dis<lost_ball_dis){
        v_x = -v_x;
        v_y=0;
        v_yaw = v_yaw;
        std::cout<<"back case\n";
    }
    if(fabs(ball_ang)<7&&ball_dis<1){
        v_x = v_x;        
        v_y = 1.5;
        v_yaw = v_yaw;
        std::cout<<"stright case\n";
    }

    if(ball_dis < hold_ball_dis && fabs(ball_ang) < hold_ball_angle){
        _LocationState = turn;
        if( cross_center_flag == true){
            _CurrentTarget--;
            cross_center_flag = false;
        }
        //_LocationState = finish;
    }
    //break;
}
int Strategy::ThroughPath(int i, int j)
{
    double Slope = (_Location->LocationPoint[i].y - _Location->LocationPoint[j].y) / (_Location->LocationPoint[i].x - _Location->LocationPoint[j].x);
    if (Slope > 999)
        Slope = 999;
    double dis = (_Location->LocationPoint[j].y - Slope * _Location->LocationPoint[j].x) / sqrt(Slope * Slope + 1);
    if (fabs(dis) < 0.0)
        return TRUE;
    else
        return FALSE;
}
std::vector<int> Strategy::OptimatePath()
{
    for (int i = 0; i < 10; i++)
    {
        _Target.TargetPoint[i].x = 0;
        _Target.TargetPoint[i].y = 0;
        _Target.TargetPoint[i].angle = 0;
        _Target.size = 0;
    }
    std::vector<int> order;
    std::vector<int> enable_point;
    for (int i = 0; i < 5; i++)
        enable_point.push_back(i);
    std::vector<int>::iterator it;
    //printf("\n");
    int horizon_point = -1;
    int hotizon_location = -1;
    int temp = -1;
    double front = 0;
    double min_rotation = 999;
    double normalization_temp_angle;
    for (int i = 0; i < 5; i++)
    {
        if (_Location->LocationPoint[i].y == 0 || _Location->LocationPoint[i].x == 0)
        {
            horizon_point = i;
            if (_Location->LocationPoint[i].y > 0)
                hotizon_location = up;
            else if (_Location->LocationPoint[i].y < 0)
                hotizon_location = down;
            else if (_Location->LocationPoint[i].x > 0)
                hotizon_location = right;
            else if (_Location->LocationPoint[i].x < 0)
                hotizon_location = left;
        }
    }
    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            if (ThroughPath(i, j))
                through_path_ary[i][j] = j + 1;
        }
    }
    switch (hotizon_location)
    {
    case up:
        // printf("up\n");
        // pick first point
        front = 90;
        order.push_back(horizon_point);
        EraseElement(enable_point, horizon_point);
        // pick second point
        front = -90;
        MinAngle(order, enable_point, front);
        // pick third point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick fourth point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick fifth point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        order.push_back(-1);
        break;
    case down:
        // printf("down\n");
        // pick first point
        front = -90;
        order.push_back(horizon_point);
        EraseElement(enable_point, horizon_point);
        // pick second point
        front = 90;
        MinAngle(order, enable_point, front);
        // pick third point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick fourth point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick fifth point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        order.push_back(-1);
        break;
    case right:
        // pick first point
        for (int i = 0; i < enable_point.size(); i++)
        {
            if (_Location->LocationPoint[enable_point[i]].angle > 0 && _Location->LocationPoint[enable_point[i]].angle < 90)
                temp = enable_point[i];
        }
        order.push_back(temp);
        EraseElement(enable_point, temp);
        // pick second point            probleming!!!!!!!!!!
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick third point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick fourth point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick fifth point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        order.push_back(-1);
        break;
    case left:
        // pick first point
        for (int i = 0; i < enable_point.size(); i++)
        {
            if (_Location->LocationPoint[enable_point[i]].angle > 90 && _Location->LocationPoint[enable_point[i]].angle < 180)
                temp = enable_point[i];
        }
        order.push_back(temp);
        EraseElement(enable_point, temp);
        // pick second point            probleming!!!!!!!!!!
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick third point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick fourth point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        // pick fifth point
        front = (_Location->LocationPoint[order.back()].angle + 180);
        Normalization(front);
        MinAngle(order, enable_point, front);
        order.push_back(-1);
        break;
    default:
        printf("UNDEFINE STATE\n");
        exit(FAULTEXECUTING);
    }
    if(order.size()>0){
        _Target.size = order.size();
        for (int i = 0; i < order.size(); i++)
        {
            if (order[i] == -1)
            {
                _Target.TargetPoint[i].x = 0;
                _Target.TargetPoint[i].y = 0;
                _Target.TargetPoint[i].angle = 0;
            }
            else
            {
                _Target.TargetPoint[i].x = _Location->LocationPoint[order[i]].x;
                _Target.TargetPoint[i].y = _Location->LocationPoint[order[i]].y;
                _Target.TargetPoint[i].angle = _Location->LocationPoint[order[i]].angle;
            }
        }
    }
    _Env->Path.size=_Target.size;
    for(int i=0; i<_Env->Path.size; i++){
        _Env->Path.TargetPoint[i].x=_Target.TargetPoint[i].x;
        _Env->Path.TargetPoint[i].y=_Target.TargetPoint[i].y;
    }
    // =================   printf final point to run  ============================
    //     for(int i=0;i<_Target.size;i++)
    //     {
    //         printf("%d : x=%lf\ty=%lf\tangle=%lf\n",i,_Target.TargetPoint[i].x,_Target.TargetPoint[i].y,_Target.TargetPoint[i].angle);
    //     }
    // =================   printf order  and   unrunning point with __ to determind   ===================
    // for (int i = 0; i < order.size(); i++)          // -1 is origin point
    // {
    //     printf("%d\t", order[i]);
    // }
    // printf("\n");
    // for (int i = 0; i < enable_point.size(); i++)
    //     printf("%d__", enable_point[i]);
    // printf("\n");
    
    return order;
}
void Strategy::showInfo(double pos_x,double pos_y,std::vector<int> order, double imu, double compensation_x, double compensation_y)
{
    std::string Sv_x = "→";
    std::string Sv_y = "↑";
    std::string Sv_yaw = "↶";
    if (_Env->Robot.v_x > 0.1)
        Sv_x = "→ ";
    else if (_Env->Robot.v_x < -0.1)
        Sv_x = "← ";
    else
        Sv_x = "";
    if (_Env->Robot.v_y > 0.1)
        Sv_y = "↑ ";
    else if (_Env->Robot.v_y < -0.1)
        Sv_y = "↓ ";
    else
        Sv_y = "";
    if (_Env->Robot.v_yaw > 2)
        Sv_yaw = "↶";
    else if (_Env->Robot.v_yaw < -2)
        Sv_yaw = "↷";
    else
        Sv_yaw = "";
    printf("***********************************************\n");
    printf("*                  START                      *\n");
    printf("***********************************************\n");

    //   ====================   test   =========================
    switch (_LocationState)
    {
    case forward:
        printf("Target : Point %d\t", order[_CurrentTarget] + 1);
        printf("State : Forward\n");
        break;
    case finish:
        printf("State : Fisish\n");
        break;
    case chase:
        printf("Target : Ball\t");
        printf("State : Chase\n");
        break;
    case turn:
        printf("Target : Point %d\t", order[_CurrentTarget] + 1);
        printf("State : Turn\n");
        break;
    case error:
        printf("State : Error\n");
        break;
    }

    if (_LocationState == forward)
        std::cout << "Target position : (" << _Target.TargetPoint[_CurrentTarget].x + compensation_x
                  << "," << _Target.TargetPoint[_CurrentTarget].y + compensation_y << ")\n";
    else if (_LocationState == chase)
        std::cout << "Target position : (" << pos_x + _Env->Robot.ball.distance * cos((_Env->Robot.pos.angle + _Env->Robot.ball.angle + 90) * DEG2RAD)
                  << "," << pos_y + _Env->Robot.ball.distance * sin((_Env->Robot.pos.angle + _Env->Robot.ball.angle + 90) * DEG2RAD)
                  << ")" << std::endl;
    else if (_LocationState == turn)
        if (_Last_state == forward)
            std::cout << "Target position : (" << _Target.TargetPoint[_CurrentTarget].x + compensation_x
                      << "," << _Target.TargetPoint[_CurrentTarget].y + compensation_y << ")\n";
        else
            std::cout << "Target position : (" << _Target.TargetPoint[_CurrentTarget].x + compensation_x
                      << "," << _Target.TargetPoint[_CurrentTarget].y + compensation_y << ")\n";

    std::cout << "Imu = " << imu << std::endl;
    std::cout << "Ball position : (" << pos_x << "," << pos_y << ")\n";
    std::string haha = Sv_x + Sv_y + Sv_yaw;
    std::cout << "Direction : " << Sv_x + Sv_y + Sv_yaw << std::endl;
    printf("Speed : (%3f,%3f,%3f)\n", _Env->Robot.v_x, _Env->Robot.v_y, _Env->Robot.v_yaw);
    printf("==================== END ======================\n\n");
}
void Strategy::showInfo(double imu, double compensation_x, double compensation_y)
{
    std::string Sv_x = "→";
    std::string Sv_y = "↑";
    std::string Sv_yaw = "↶";
    if (_Env->Robot.v_x > 0.1)
        Sv_x = "→ ";
    else if (_Env->Robot.v_x < -0.1)
        Sv_x = "← ";
    else
        Sv_x = "";
    if (_Env->Robot.v_y > 0.1)
        Sv_y = "↑ ";
    else if (_Env->Robot.v_y < -0.1)
        Sv_y = "↓ ";
    else
        Sv_y = "";
    if (_Env->Robot.v_yaw > 2)
        Sv_yaw = "↶";
    else if (_Env->Robot.v_yaw < -2)
        Sv_yaw = "↷";
    else
        Sv_yaw = "";
    printf("***********************************************\n");
    printf("*                  START                      *\n");
    printf("***********************************************\n");

    switch (_LocationState)
    {
    case forward:
        printf("Target : Point %d\t", _CurrentTarget);
        printf("State : Forward\n");
        break;
    case back:
        printf("Target : Point %d\t", _CurrentTarget);
        printf("State : Back\n");
        break;
    case finish:
        printf("State : Fisish\n");
        break;
    case chase:
        printf("Target : Ball\t");
        printf("State : Chase\n");
        break;
    case turn:
        printf("Target : Point %d\t", _CurrentTarget);
        printf("State : Turn\n");
        break;
    case error:
        printf("State : Error\n");
        break;
    }
    if (_LocationState == forward)
        std::cout << "Target position : (" << _Location->LocationPoint[_CurrentTarget].x + compensation_x
                  << "," << _Location->LocationPoint[_CurrentTarget].y + compensation_y << ")\n";
    else if (_LocationState == back)
        std::cout << "Target position : (" << _Location->MiddlePoint[_CurrentTarget].x + compensation_x
                  << "," << _Location->MiddlePoint[_CurrentTarget].y + compensation_y << ")\n";
    else if (_LocationState == chase)
        std::cout << "Target position : (" << _Env->Robot.pos.x + _Env->Robot.ball.distance * cos((_Env->Robot.pos.angle + _Env->Robot.ball.angle + 90) * DEG2RAD)
                  << "," << _Env->Robot.pos.y + _Env->Robot.ball.distance * sin((_Env->Robot.pos.angle + _Env->Robot.ball.angle + 90) * DEG2RAD)
                  << ")" << std::endl;
    else if (_LocationState == turn)
        if (_Last_state == forward)
            std::cout << "Target position : (" << _Location->MiddlePoint[_CurrentTarget].x + compensation_x
                      << "," << _Location->MiddlePoint[_CurrentTarget].y + compensation_y << ")\n";
        else
            std::cout << "Target position : (" << _Location->MiddlePoint[_CurrentTarget].x + compensation_x
                      << "," << _Location->MiddlePoint[_CurrentTarget].y + compensation_y << ")\n";
    std::cout << "Imu = " << imu << std::endl;
    std::cout << "Robot position : (" << _Env->Robot.pos.x << "," << _Env->Robot.pos.y << ")\n";
    std::string haha = Sv_x + Sv_y + Sv_yaw;
    std::cout << "Direction : " << Sv_x + Sv_y + Sv_yaw << std::endl;
    printf("Speed : (%3f,%3f,%3f)\n", _Env->Robot.v_x, _Env->Robot.v_y, _Env->Robot.v_yaw);
    printf("==================== END ======================\n\n");
}
void Strategy::Normalization(double &angle)
{
    if (angle > 180)
        angle -= 360;
    else if (angle < -180)
        angle += 360;
}
void Strategy::EraseElement(std::vector<int> &vec, int index)
{
    for (int i = 0; i < vec.size(); i++)
    {
        if (vec[i] == index)
        {
            index = i;
            break;
        }
    }
    std::vector<int>::iterator it = vec.begin() + index;
    vec.erase(it);
}
void Strategy::MinAngle(std::vector<int> &order, std::vector<int> &enable_point, int front)
{
    int backflag = 1;
    int temp;
    double min_rotation = 999;
    double normalization_temp_angle;
    for (int i = 0; i < enable_point.size(); i++)
    {
        normalization_temp_angle = fabs(_Location->LocationPoint[enable_point[i]].angle - front);
        Normalization(normalization_temp_angle);
        if (fabs(normalization_temp_angle) < min_rotation)
        {
            min_rotation = fabs(normalization_temp_angle);
            temp = enable_point[i];
        }
        if (through_path_ary[order.back()][enable_point[i]])
        {
            temp = enable_point[i];
            backflag = 0;
        }
    }
    if (backflag)
        order.push_back(-1);
    order.push_back(temp);
    EraseElement(enable_point, temp);
}
