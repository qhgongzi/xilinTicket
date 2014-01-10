#include "stdafx.h"
#include "echttp/http.hpp"
#include "Huoche.h"
#include <iostream>
#include <fstream>
#include "BuyTicketDlg.h"
#include "YzDlg.h"
#include <WinInet.h>
#include "xxtea.h"
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "Ticket.hpp"

#pragma comment(lib,"Wininet.lib")

boost::random::mt19937 rand_gen;

std::string CHuoche::xxtea_encode(std::string data_,std::string key_)
{
    xxtea_long ret_len=0;
    boost::shared_array<char> v(new char[data_.size()*2]);//*2 because need enough to storage buf in encode inside.
    memset(v.get(),0,data_.size()*2);
    memcpy(v.get(),data_.c_str(),data_.size());

    boost::shared_array<char> k(new char[key_.size()*2]);
    memset(k.get(),0,key_.size()*2);
    memcpy(k.get(),key_.c_str(),key_.size());

    unsigned char * ret=xxtea_encrypt((unsigned char *)v.get(),data_.size(),(unsigned char *)k.get(),&ret_len);

    char* buf=new char[ret_len*2+1];
    memset(buf,0,ret_len+1);

    for( int i = 0; i < ret_len; ++i)
    {
        unsigned char c=*(ret+i);
        sprintf(buf+2*i, "%02x", c);
    }

    std::string result=echttp::base64_encode((unsigned char*)buf,ret_len*2);
    delete[] buf;
    free(ret);

    return result;
}


CHuoche::CHuoche(CBuyTicketDlg *dlg)
	: isInBuy(false)
	, m_Success(false)
{
	this->dlg=dlg;
	http=new echttp::http();
    this->buy_list=new std::queue<Ticket>();
    this->LoadStation();

	http->Request.set_defalut_userAgent("Mozilla/5.0 (compatible; MSIE 9.0; qdesk 2.5.1177.202; Windows NT 6.1; WOW64; Trident/6.0)");
	this->LoadDomain();

	this->http->Get("https://"+this->domain+"/otn/");
	this->http->Request.m_header.insert("Referer","https://kyfw.12306.cn/otn/");


	std::string initUrl=this->http->Get("https://"+this->domain+"/otn/login/init")->as_string();

    if(initUrl.find("/otn/dynamicJs/loginJs")!=std::string::npos)
    {
        std::string loginjs=echttp::substr(initUrl,"/otn/dynamicJs/loginJs","\"");
	    std::string loginjs2=echttp::substr(initUrl,"/otn/resources/merged/login_js.js?scriptVersion=","\"");

	    this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/login/init");
	    this->http->Get("https://"+this->domain+"/otn/resources/merged/login_js.js?scriptVersion="+loginjs2);
	    std::string ret= this->http->Get("https://"+this->domain+"/otn/dynamicJs/loginJs"+loginjs)->as_string();
        this->encrypt_code(ret);

		//�ж��Ƿ�������������url
		std::string ready_str=echttp::substr(ret,"$(document).ready(function(){","success");
		if(ready_str.find("jq({url :'")!=std::string::npos)
		{
			std::string url=echttp::substr(ready_str,"jq({url :'","'");
			this->http->Post("https://"+this->domain+""+url,"");
		}

    }else
    {
        this->dlg->m_listbox.AddString("��ȡ��¼��Ϣ�쳣��");
    }
}


CHuoche::~CHuoche(void)
{
	delete http;
}

//�Ӽ���js������֤��Ϣ
void CHuoche::encrypt_code(std::string src)
{
    if(src.find("gc(){var key='")==std::string::npos){
        this->encrypt_str="";
    }else{  
        std::string key=echttp::substr(src,"gc(){var key='","';");
        std::string code=this->xxtea_encode("1111",key);

        this->encrypt_str="&"+echttp::UrlEncode(key)+"="+echttp::UrlEncode(code);
    }

}

bool CHuoche::Login(std::string username, std::string password, std::string code)
{
	this->uname=username;
	this->upass=password;
	this->yzcode=code;
	std::ofstream file("c:\\��¼����.txt",std::ios::app);
	this->http->Request.m_header.insert("x-requested-with","XMLHttpRequest");
	//std::string num=this->getSuggest();
    std::string pstr="loginUserDTO.user_name="+this->uname+"&userDTO.password="+this->upass+"&randCode="+this->yzcode;
	
	
	std::string res=this->http->Post("https://"+this->domain+"/otn/login/loginAysnSuggest",pstr)->as_string();
	res=echttp::Utf8Decode(res);

	if(res.find("{\"loginCheck\":\"Y\"}")!=std::string::npos){
		this->dlg->m_listbox.AddString("��¼�ɹ�");
		this->SetCookie(this->http->Request.m_cookies.cookie_string());
        this->http->Post("https://"+this->domain+"/otn/login/init","_json_att=");
		return true;
    }else if(res.find("��֤�벻��ȷ")!=std::string::npos){
        this->dlg->m_listbox.AddString("��֤�벻��ȷ");
		return false;
    }else{
		file<<res;
		file.close();
		this->showMsg("��¼ʧ�ܣ�"+echttp::substr(res,"messages\":[\"","]"));
		return false;
	}
	
}


bool CHuoche::GetCode(void)
{
	this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/login/init");
	int status_code=this->http->Get(std::string("https://"+this->domain+"/otn/passcodeNew/getPassCodeNew?module=login&rand=sjrand"),std::string("c:\\buyticket.png"))->status_code;
	return status_code==200;
}


std::string CHuoche::getSuggest(void)
{
	std::string res=this->http->Post("https://dynamic.12306.cn/otsweb/loginAction.do?method=loginAysnSuggest","")->as_string();
	if(res=="") return "";
	return echttp::substr(res,"{\"loginRand\":\"","\"");
}

//���ʲ�ѯ��Ʊ��Ϣҳ�棨��ʱ���ʣ��Ա������ߣ�
void CHuoche::SerachTicketPage()
{
	this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/leftTicket/init");
    this->http->Get("https://"+this->domain+"/otn/leftTicket/init",boost::bind(&CHuoche::RecvSearchTicketPage,this,_1));
    this->CheckUserOnline();
}

void CHuoche::RecvSearchTicketPage(boost::shared_ptr<echttp::respone> respone)
{
    std::string sources;
	if (respone->as_string()!=""){
		sources=respone->as_string();

        if(sources.find("/otn/dynamicJs/queryJs")!=std::string::npos)
        {
            std::string authJs=echttp::substr(sources,"/otn/dynamicJs/queryJs","\"");

	        this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/leftTicket/init");
	        std::string ret= this->http->Get("https://"+this->domain+"/otn/dynamicJs/queryJs"+authJs)->as_string();
            this->encrypt_code(ret);

		    //�ж��Ƿ�������������url
		    std::string ready_str=echttp::substr(ret,"$(document).ready(function(){","success");
		    if(ready_str.find("jq({url :'")!=std::string::npos)
		    {
			    std::string url=echttp::substr(ready_str,"jq({url :'","'");
			    this->http->Post("https://"+this->domain+""+url,"");
		    }
		
        }else
        {
            this->showMsg("��ȡ��ѯ��Ʊҳ���쳣��");
        }

    }else{
        this->showMsg("��Ʊҳ���ѿհ�!!!");
    }
}

void CHuoche::SearchTicket(std::string fromStation,std::string toStation,std::string date)
{
    this->LoadDomain();

	//http->Request.set_defalut_userAgent("Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; WOW64; Trident/6.0)");

	this->fromCode=station[fromStation];
	this->toCode=station[toStation];
	this->date=date;
	this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/leftTicket/init");

    this->http->Request.m_header.insert("x-requested-with","XMLHttpRequest");
    this->http->Request.m_header.insert("Content-Type","application/x-www-form-urlencoded");
	std::string url="https://"+this->domain+"/otn/leftTicket/query?leftTicketDTO.train_date="+this->date
                    +"&leftTicketDTO.from_station="+this->fromCode+"&leftTicketDTO.to_station="+this->toCode
                    +"&purpose_codes=ADULT";
	this->http->Get(url,boost::bind(&CHuoche::RecvSchPiao,this,_1));

	//5 percent to flush search page; 
	boost::random::uniform_int_distribution<> dist(1, 60);
	if(dist(rand_gen)==20)
	{
		this->SerachTicketPage();
	}

}

bool CHuoche::isTicketEnough(std::string tickstr)
{
	if(tickstr=="--" || tickstr=="*" || tickstr.find("��")!=std::string::npos)
		return false;

	if(tickstr.find("��")!=std::string::npos)
		return true;

	CString fullname2;
	this->dlg->GetDlgItem(IDC_FULLNAME2)->GetWindowText(fullname2);
	fullname2.Trim();

	CString fullname3;
	this->dlg->GetDlgItem(IDC_FULLNAME2)->GetWindowText(fullname3);
	fullname3.Trim();

	int ticket_num=atoi(tickstr.c_str());

	if(!fullname3.IsEmpty())
	{
		return ticket_num>=3;
	}
	else if(!fullname2.IsEmpty())
	{
		return ticket_num>=2;
	}else
	{
		return ticket_num>=1;
	}

}

void CHuoche::RecvSchPiao(boost::shared_ptr<echttp::respone> respone)
{
	std::string restr;
	
	restr=echttp::Utf8Decode(respone->as_string());

	if(restr!=""&&restr!="-10" &&restr.find("queryLeftNewDTO")!=std::string::npos){
		std::ofstream webfile("c:\\web.txt",std::ios::app);
		webfile<<restr<<"\r\n";
		webfile.close();

		while(restr.find("queryLeftNewDTO")!=std::string::npos)
		{
            std::string ticketInfo=echttp::substr(restr,"queryLeftNewDTO","{\"queryLeftNewDTO");

            Ticket ticket(ticketInfo);

			restr=restr.substr(restr.find("queryLeftNewDTO")+5);

			std::string trainstr=echttp::substr(restr,"_train_code\":\"","\"");
			if(this->train!=""&&this->train.find(trainstr)==std::string::npos)
			{
				continue;
			}

            if(this->isTicketEnough(ticket.first_seat)&&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_YDZ))
			{
				if(restr.find("secretStr")!=std::string::npos)
				{
					this->showMsg(trainstr+"��һ����"+"---��Ʊ��Ŀ:"+ticket.first_seat);
                    ticket.SetBuySeat("M");
                    buy_list->push(ticket);
				}else{
					this->showMsg("δ֪һ������Ʊ��Ϣ");
				}
				

			}
            
			if(this->isTicketEnough(ticket.second_seat)&&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_EDZ))
			{
				if(restr.find("secretStr")!=std::string::npos)
				{
                    ticket.SetBuySeat("O");
                    buy_list->push(ticket);
					this->showMsg(trainstr+"�ж�����"+"---��Ʊ��Ŀ:"+ticket.second_seat);
				}else{
					this->showMsg("δ֪��������Ʊ��Ϣ");
				}
				

			}
			
			if(this->isTicketEnough(ticket.soft_bed)&&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_RW))
			{
				if(restr.find("secretStr")!=std::string::npos)
				{
					ticket.SetBuySeat("4");
                    buy_list->push(ticket);
					this->showMsg(trainstr+"������"+"---��Ʊ��Ŀ:"+ticket.soft_bed);
				}else{
					this->showMsg("δ֪������Ʊ��Ϣ");
				}
				

			}
            
			if(this->isTicketEnough(ticket.hard_bed)&&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_YW))
			{
				if(restr.find("secretStr")!=std::string::npos)
				{
					ticket.SetBuySeat("3");
                    buy_list->push(ticket);
                    this->showMsg(trainstr+"������"+"---��Ʊ��Ŀ:"+ticket.hard_bed);
				}else{
					this->showMsg("δ֪������Ʊ��Ϣ");
				}
				

            }
			
			if(this->isTicketEnough(ticket.hard_seat) &&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_YZ))
			{
                this->showMsg(trainstr+"��Ӳ��"+"---��Ʊ��Ŀ:"+ticket.hard_seat);
				
				if(restr.find("secretStr")!=std::string::npos)
				{
					ticket.SetBuySeat("1");
                    buy_list->push(ticket);
				}else{
					this->showMsg("δ֪Ӳ����Ϣ");
				}
				
			}

		}

		//�����⵽��Ӧ��Ʊ�����µ�
        int queue_size=buy_list->size();
		if(queue_size>0 && !buy_list->empty()){

            Ticket task_ticket=buy_list->front();
            this->submitOrder(task_ticket);
            this->showMsg("��ʼ����:"+task_ticket.station_train_code);

            while (buy_list->size()>0)
            {
                if(queue_size==buy_list->size()) //�����ǰ����������֮ǰ�����Ķ�����һ�£�����������ִ��
                {    
                    continue;
                    Sleep(50);
                }

                queue_size=buy_list->size();
				if(!buy_list->empty())
				{
					Ticket task_ticket=buy_list->front();
					this->submitOrder(task_ticket);
					this->showMsg("��ʼ����:"+task_ticket.station_train_code);
				}
            }
		}else{
			this->showMsg("û�����̻���Ӳ��");
		}

	}else{
		this->showMsg("���������������:"+echttp::Utf8Decode(echttp::substr(restr,"\"messages\":\[\"","\"")));
	}
	

	

}


void CHuoche::showMsg(std::string msg)
{

	CTime tm;
	tm=CTime::GetCurrentTime();
	string str=tm.Format("%m��%d�� %X��");
	str+=msg;
	dlg->m_listbox.InsertString(0,str.c_str());

	std::ofstream file("c:\\ticketlog.txt",std::ios::app);
	file<<str<<"\r\n";
	file.close();

}


bool CHuoche::submitOrder(Ticket ticket)
{
	this->isInBuy=true;

    std::string pstr="secretStr="+ticket.secret_str+"&train_date="+ticket.train_date+"&back_train_date=2014-01-01&tour_flag=dc&purpose_codes=ADULT"+
        "&query_from_station_name="+ticket.from_station_name+"&query_to_station_name="+ticket.to_station_name+"&undefined";

	this->showMsg("������:"+ticket.station_train_code);

	boost::shared_ptr<echttp::respone> ret=this->http->Post("https://"+this->domain+"/otn/leftTicket/submitOrderRequest",pstr);
	std::string recvStr=ret->as_string();
    if(recvStr.find("status\":true")!=std::string::npos)
	{
		this->http->Post("https://"+this->domain+"/otn/confirmPassenger/initDc","_json_att=",boost::bind(&CHuoche::RecvSubmitOrder,this,_1,ticket));
	}else{
		this->showMsg("Ԥ������!"+echttp::Utf8Decode(echttp::substr(recvStr,"\"messages\":\[\"","\"")));
        this->buy_list->pop();
	}
	
	return false;
}

void CHuoche::RecvSubmitOrder(boost::shared_ptr<echttp::respone> respone,Ticket ticket)
{
	std::string restr;
	restr=echttp::Utf8Decode(respone->as_string());

    this->LoadPassanger();

	this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/confirmPassenger/initDc");

	
	//if(restr.find("/otsweb/dynamicJsAction.do")!=std::string::npos)
 //   {
 //       std::string authJs=echttp::substr(restr,"/otsweb/dynamicJsAction.do","\"");
	//    std::string ret= this->http->Get("https://dynamic.12306.cn/otsweb/dynamicJsAction.do"+authJs)->as_string();

	//	//�ж��Ƿ�������������url
	//	std::string ready_str=echttp::substr(ret,"$(document).ready(function(){","success");
	//	if(ready_str.find("jq({url :'")!=std::string::npos)
	//	{
	//		std::string url=echttp::substr(ready_str,"jq({url :'","'");
	//		this->http->Post("https://dynamic.12306.cn"+url,"");
	//	}
 //   }

	string TOKEN=echttp::substr(restr,"globalRepeatSubmitToken = '","'");
    string keyCheck=echttp::substr(restr,"'key_check_isChange':'","'");
	string leftTicketStr=echttp::substr(restr,"leftTicketStr':'","'");
    string trainLocation=echttp::substr(restr,"train_location':'","'");
	
	
	string checkbox2="";

	string seattype=ticket.seat_type;//��λ���� 3Ϊ���� 1ΪӲ��
	
	
	std::string code_path=this->loadCode2();
	CYzDlg yzDlg;
	yzDlg.huoche=this;//���ݱ���ָ�뵽�Ի���
    yzDlg.file_path=code_path;
	if(yzDlg.DoModal()){
		this->showMsg("��ʱһ�£�����ᱻ�⣡");
		//Sleep(1000);
checkcode:
		std::string randcode=yzDlg.yzcode;
		/*std::string user2info="oldPassengers=&checkbox9=Y";
		this->http->Get("https://dynamic.12306.cn/otsweb/order/traceAction.do?method=logClickPassenger&passenger_name="+myName+"&passenger_id_no="+IdCard+"&action=checked");
		if(myName2!="")
		{
			user2info="passengerTickets="+seattype+"%2C0%2C1%2C"+myName2+"%2C1%2C"+IdCard2+"%2C"+Phone2
				+"%2CY&oldPassengers="+myName2+"%2C1%2C"+IdCard2+"&passenger_2_seat="
				+seattype+"&passenger_2_ticket=1&passenger_2_name="
				+myName2+"&passenger_2_cardtype=1&passenger_2_cardno="
				+IdCard2+"&passenger_2_mobileno="+Phone2
				+"&checkbox9=Y";
			checkbox2="&checkbox1=1";

			this->http->Get("https://dynamic.12306.cn/otsweb/order/traceAction.do?method=logClickPassenger&passenger_name="+myName2+"&passenger_id_no="+IdCard2+"&action=checked");

		}*/
		std::string passanger_info;

		if(passanger_name2!="")
		{
			passanger_info=seattype+"%2C0%2C1%2C"+this->passanger_name+"%2C1%2C"+this->passanger_idcard+"%2C"+this->passanger_phone
            +"%2CN_"+seattype+"%2C0%2C1%2C"+this->passanger_name1+"%2C1%2C"+this->passanger_idcard1+"%2C"+this->passanger_phone1
            +"%2CN_"+seattype+"%2C0%2C1%2C"+this->passanger_name2+"%2C1%2C"+this->passanger_idcard2+"%2C"+this->passanger_phone2
            +"%2CN&oldPassengerStr="+passanger_name+"%2C1%2C"+passanger_idcard+"%2C1_"+passanger_name1+"%2C1%2C"+passanger_idcard1+"%2C1_"+passanger_name2+"%2C1%2C"+passanger_idcard2+"%2C1_";
		}
		else if(passanger_name1=="")
		{
			passanger_info=seattype+"%2C0%2C1%2C"+this->passanger_name+"%2C1%2C"+this->passanger_idcard+"%2C"+this->passanger_phone
            +"%2CN_"+seattype+"%2C0%2C1%2C"+this->passanger_name1+"%2C1%2C"+this->passanger_idcard1+"%2C"+this->passanger_phone1
            +"%2CN&oldPassengerStr="+passanger_name+"%2C1%2C"+passanger_idcard+"%2C1_"+passanger_name1+"%2C1%2C"+passanger_idcard1+"%2C1_";
					
		}else{
			passanger_info=seattype+"%2C0%2C1%2C"+this->passanger_name+"%2C1%2C"+this->passanger_idcard+"%2C"+this->passanger_phone
            +"%2CN&oldPassengerStr="+passanger_name+"%2C1%2C"+passanger_idcard+"%2C1_";
		}

        std::string pstr="cancel_flag=2&bed_level_order_num=000000000000000000000000000000&passengerTicketStr="+passanger_info+"&tour_flag=dc&randCode="+randcode
            +"&_json_att=&REPEAT_SUBMIT_TOKEN="+TOKEN;

		string url="https://"+this->domain+"/otn/confirmPassenger/checkOrderInfo";

		string checkstr=this->http->Post(url,pstr)->as_string();
		checkstr=echttp::Utf8Decode(checkstr);

		if(checkstr.find("submitStatus\":true")!=std::string::npos 
			&& checkstr.find("status\":true")!=std::string::npos)
		{
            //����ŶӶ��У���ֹ���ύ
            bool ticket_enough=this->CheckQueueCount(ticket,TOKEN);
			
            if(!ticket_enough){
                this->showMsg("���ź�,"+ticket.station_train_code+"�Ŷ���������");
                buy_list->pop();
                return;
            }

			this->http->Request.m_header.insert("Content-Type","application/x-www-form-urlencoded; charset=UTF-8");
			this->http->Request.m_header.insert("X-Requested-With","XMLHttpRequest");

            pstr="passengerTicketStr="+passanger_info+"&randCode="+randcode+"&purpose_codes=00"+
            +"&key_check_isChange="+keyCheck+"&leftTicketStr="+leftTicketStr+"&train_location="+trainLocation
            +"&_json_att=&REPEAT_SUBMIT_TOKEN="+TOKEN;
            //�ύ����
			this->http->Post("https://"+this->domain+"/otn/confirmPassenger/confirmSingleForQueue",pstr,boost::bind(&CHuoche::confrimOrder,this,_1,pstr));
		}else if(checkstr.find("������")!=std::string::npos){
			this->showMsg(checkstr);
			Sleep(1000);
			goto checkcode;
		}
		else if(checkstr.find("��֤��")!=std::string::npos)
		{
			this->showMsg("��֤��������������룡");
			int ret=yzDlg.DoModal();
			if(ret==IDOK){
				goto checkcode;
			}else if(ret==IDCANCEL) {
				this->showMsg("��ѡ����ȡ����Ʊ��");
                this->buy_list->pop();
			}
		}
		else{
            this->showMsg("��鶩����������:"+echttp::substr(checkstr,"errMsg","}"));
            this->buy_list->pop();
		}


	}

}

void CHuoche::SetCookie(std::string cookies)
{
	while(cookies.find(";")!=std::string::npos)
	{
		std::string cookie=cookies.substr(0,cookies.find_first_of(" "));
		cookie=cookie+"expires=Sun,22-Feb-2099 00:00:00 GMT";
		::InternetSetCookieA("https://kyfw.12306.cn",NULL,cookie.c_str());
		cookies=cookies.substr(cookies.find_first_of(" ")+1);
	}
}

std::string CHuoche::loadCode2(void)
{
    boost::random::uniform_int_distribution<> dist1(1, 50000);
    rand_gen.seed(time(0));
    int randcode=dist1(rand_gen);
	std::string randstr=echttp::convert<std::string>(randcode);
    std::string yz_code="c:\\12306\\buyticket"+randstr+".png";

	this->http->Request.m_header.insert("Referer","https://kyfw.12306.cn/otn/confirmPassenger/initDc");
	this->http->Get(std::string("https://"+this->domain+"/otn/passcodeNew/getPassCodeNew?module=passenger&rand=randp"),yz_code);
    return yz_code;
}


void CHuoche::confrimOrder(boost::shared_ptr<echttp::respone> respone,std::string pstr)
{
	std::string result;
	if (respone->as_string()!=""){
		result=echttp::Utf8Decode(respone->as_string());
		if(result.find("status\":true")!=std::string::npos)
		{
				this->m_Success=true;
				this->showMsg("!!!!!Ԥ���ɹ������ٵ�12306����");
				this->buy_list->pop();
		}else if(result.find("������")!=std::string::npos)
		{
			if(!this->m_Success){
				Sleep(1000);
				this->http->Post("https://"+this->domain+"/otn/confirmPassenger/confirmSingleForQueue",pstr,boost::bind(&CHuoche::confrimOrder,this,_1,pstr));
				this->showMsg(result+"������!");
			}
		}
		else
		{
				this->showMsg(result);
				this->buy_list->pop();
		}
	}else{
		if(!this->m_Success){
			this->http->Post("https://"+this->domain+"/otn/confirmPassenger/confirmSingleForQueue",pstr,boost::bind(&CHuoche::confrimOrder,this,_1,pstr));
			this->showMsg("���ؿգ�������!");
		}
	}
	

	return ;
}


void CHuoche::LoadStation(void)
{

    station["������"]="VAP";
	station["������"]="BOP";
	station["����"]="BJP";
	station["������"]="VNP";
	station["������"]="BXP";
	station["���챱"]="CUW";
	station["����"]="CQW";
	station["������"]="CRW";
	station["�Ϻ�"]="SHH";
	station["�Ϻ���"]="SNH";
	station["�Ϻ�����"]="AOH";
	station["�Ϻ���"]="SXH";
	station["���"]="TBP";
	station["���"]="TJP";
	station["�����"]="TIP";
	station["�����"]="TXP";
	station["����"]="CCT";
	station["������"]="CET";
	station["������"]="CRT";
	station["�ɶ���"]="ICW";
	station["�ɶ���"]="CNW";
	station["�ɶ�"]="CDW";
	station["��ɳ"]="CSQ";
	station["��ɳ��"]="CWQ";
	station["����"]="FZS";
	station["������"]="FYS";
	station["����"]="GIW";
	station["���ݱ�"]="GBQ";
	station["���ݶ�"]="GGQ";
	station["����"]="GZQ";
	station["������"]="IZQ";
	station["������"]="HBB";
	station["��������"]="VBB";
	station["��������"]="VAB";
	station["�Ϸ�"]="HFH";
	station["�Ϸ���"]="HTH";
	station["���ͺ��ض�"]="NDC";
	station["���ͺ���"]="HHC";
	station["���ڶ�"]="HMQ";
	station["����"]="VUQ";
	station["����"]="HZH";
	station["������"]="XHH";
    station["���ݶ�"]="HGH";
	station["����"]="JNK";
	station["���϶�"]="JAK";
	station["������"]="JGK";
	station["����"]="KMM";
	station["������"]="KXM";
	station["����"]="LSO";
	station["���ݶ�"]="LVJ";
	station["����"]="LZJ";
	station["������"]="LAJ";
	station["�ϲ�"]="NCG";
	station["�Ͼ�"]="NJH";
	station["�Ͼ���"]="NKH";
	station["����"]="NNZ";
	station["ʯ��ׯ��"]="VVP";
	station["ʯ��ׯ"]="SJP";
	station["����"]="SYT";
	station["������"]="SBT";
	station["������"]="SDT";
	station["̫ԭ��"]="TBV";
	station["̫ԭ��"]="TDV";
	station["̫ԭ"]="TYV";
	station["�人"]="WHN";
	station["����Ӫ��"]="KNM";
	station["��³ľ��"]="WMR";
	station["������"]="EAY";
	station["����"]="XAY";
	station["������"]="CAY";
	station["������"]="XXO";
	station["����"]="YIJ";
	station["֣��"]="ZZF";
	station["����ɽ"]="ART";
	station["����"]="AKY";
	station["������"]="ASR";
	station["�����"]="AHX";
	station["����ɽ��"]="AKR";
	station["��ƽ"]="APT";
	station["����"]="AQH";
	station["��˳"]="ASW";
	station["��ɽ"]="AST";
	station["����"]="AYF";
	station["����"]="BAB";
	station["����"]="BBH";
	station["�׳�"]="BCT";
	station["����"]="BHZ";
	station["�׺�"]="BEL";
	station["�׽�"]="BAP";
	station["����"]="BJY";
	station["����"]="BJB";
	station["����ͼ"]="BKX";
	station["��ɫ"]="BIZ";
	station["��ɽ��"]="HJL";
	station["��̨"]="BTT";
	station["��ͷ��"]="BDC";
	station["��ͷ"]="BTC";
	station["������"]="BXR";
	station["��Ϫ"]="BXT";
	station["���ƶ���"]="BEC";
	station["������"]="BXJ";
	station["����"]="BZH";
	station["���"]="CBN";
	station["����"]="VGQ";
	station["�е�"]="CDP";
	station["����"]="CDT";
	station["���"]="CFD";
	station["����"]="CDG";
	station["����"]="CEH";
	station["��ƽ"]="CPP";
	station["����"]="CRG";
	station["��ͼ"]="CTT";
	station["��͡��"]="CDB";
	station["����"]="CIJ";
	station["����"]="CXK";
	station["����"]="COM";
	station["������"]="CXT";
	station["���α�"]="CBF";
	station["����"]="CZJ";
	station["����"]="IYH";
	station["����"]="CZH";
	station["����"]="CZQ";
	station["����"]="CZF";
	station["����"]="COP";
	station["����"]="CZZ";
	station["�󰲱�"]="RNT";
	station["���"]="DCT";
	station["����"]="DUT";
	station["������"]="DFB";
	station["��ݸ��"]="DMQ";
	station["��ɽ"]="DHD";
	station["�ػ�"]="DHJ";
	station["�ػ�"]="DHL";
	station["�»�"]="DHT";
	station["������"]="DJB";
	station["��"]="DFP";
	station["������"]="DDW";
	station["������"]="DFT";
	station["����"]="DKM";
	station["����"]="DLT";
	station["����"]="DNG";
	station["����"]="DZX";
	station["��ʤ"]="DOC";
	station["��ʯ��"]="DQT";
	station["��ͬ"]="DTV";
	station["��Ӫ"]="DPK";
	station["������"]="DUX";
	station["����"]="RYW";
	station["����"]="DOF";
	station["����"]="RXW";
	station["����"]="DZP";
	station["�����"]="EJC";
	station["����"]="RLC";
	station["��ʩ"]="ESN";
	station["���Ǹ�"]="FEZ";
	station["����"]="FES";
	station["�����"]="FLV";
	station["����"]="FLW";
	station["��������"]="FRX";
	station["��˳��"]="FET";
	station["��ɽ"]="FSQ";
	station["����"]="FXD";
	station["����"]="FYH";
	station["���ľ"]="GRO";
	station["�㺺"]="GHW";
	station["�Ž�"]="GJV";
	station["���ֱ�"]="GBZ";
	station["����"]="GRX";
	station["����"]="GLZ";
	station["��ʼ"]="GXN";
	station["��ˮ"]="GSN";
	station["����"]="GNJ";
	station["��Ԫ"]="GYW";
	station["����"]="GZG";
	station["������"]="GLT";
	station["��������"]="GBT";
	station["����"]="AUH";
	station["�ױ�"]="HMB";
	station["����"]="HRH";
	station["����"]="HVN";
	station["�ӱ�"]="HBV";
	station["�괨"]="KCN";
	station["����"]="HCY";
	station["����"]="HDP";
	station["�������"]="HDB";
	station["�׸�"]="HGB";
	station["�ʹ���"]="HTT";
	station["���"]="HEM";
	station["�ں�"]="HJB";
	station["����"]="HHQ";
	station["����"]="HKN";
	station["��«��"]="HLD";
	station["������"]="HRX";
	station["���ֹ���"]="HWD";
	station["����"]="HLB";
	station["����"]="HMV";
	station["����"]="HMR";
	station["����"]="HAH";
	station["����"]="HNB";
	station["������"]="EUH";
	station["����"]="HQM";
	station["���ᱱ"]="HBP";
	station["����"]="HRP";
	station["��ʯ��"]="OSN";
	station["��ɽ"]="HSY";
	station["��ʯ"]="HSN";
	station["��ɽ"]="HKH";
	station["��ˮ"]="HSP";
	station["����"]="HYQ";
	station["����"]="HIK";
	station["����"]="HXZ";
	station["����"]="HOY";
	station["����"]="HCQ";
	station["����"]="VAG";
	station["����"]="JAL";
	station["���ߴ�"]="JBG";
	station["����"]="JCF";
	station["��ǽ�"]="JJZ";
	station["������"]="JCG";
	station["�η�"]="JFF";
	station["�Ӹ����"]="JGX";
	station["����ɽ"]="JGG";
	station["�Ժ�"]="JHL";
	station["����"]="RNH";
	station["����"]="JBH";
	station["�Ž�"]="JJG";
	station["����"]="JLL";
	station["����"]="JMN";
	station["��ľ˹"]="JMB";
	station["����"]="JIK";
	station["������"]="JAC";
	station["��Ȫ"]="JQJ";
	station["��ɽ"]="JUH";
	station["����"]="JIQ";
	station["��̨"]="JTL";
	station["����ɽ"]="JVJ";
	station["����"]="JXB";
	station["����"]="JKP";
	station["��Ϫ��"]="JRH";
	station["������"]="JGJ";
	station["����"]="JFW";
	station["����"]="JZD";
	station["����"]="JZT";
	station["�����"]="KLR";
	station["����"]="KFF";
	station["��"]="KLV";
	station["����"]="KLW";
	station["��ʲ"]="KSR";
	station["��ɽ��"]="KNH";
	station["����"]="KTR";
	station["��ԭ"]="KYT";
	station["����"]="UAH";
	station["�鱦"]="LBF";
	station["«����"]="UCH";
	station["¡��"]="LCW";
	station["½��"]="LKZ";
	station["����"]="LCN";
	station["�ٴ�"]="LCG";
	station["º��"]="UTP";
	station["¹��"]="LDL";
	station["¦��"]="LDQ";
	station["�ٷ�"]="LFV";
	station["����ׯ"]="LGP";
	station["�ٺ�"]="LHC";
	station["���"]="LON";
	station["�̻�"]="LWJ";
	station["¡��"]="UHP";
	station["����"]="LHM";
	station["�ٽ�"]="LQL";
	station["����"]="LJL";
	station["����"]="LHV";
	station["����"]="LLG";
	station["��ƽ"]="UPP";
	station["����ˮ"]="UMW";
	station["����"]="LVV";
	station["��˳"]="LST";
	station["¤��"]="LXJ";
	station["���"]="LEQ";
	station["��Ϫ"]="LWH";
	station["����"]="UEP";
	station["����"]="LYQ";
	station["����"]="LYF";
	station["����"]="LYS";
	station["������"]="LDF";
	station["���Ƹ۶�"]="UKH";
	station["����"]="LVK";
	station["��������"]="LLF";
	station["��԰"]="DHR";
	station["��Դ"]="LYD";
	station["��Դ"]="LYL";
	station["��־"]="LZX";
	station["����"]="LZZ";
	station["����"]="LZD";
	station["���"]="MCN";
	station["��ɺ�"]="MDX";
	station["ĵ����"]="MDB";
	station["Ī������"]="MRX";
	station["����"]="MHX";
	station["����"]="MGH";
	station["Į��"]="MVX";
	station["÷��"]="MKQ";
	station["ï����"]="MDQ";
	station["ï��"]="MMZ";
	station["��ɽ"]="MSB";
	station["������"]="MJT";
	station["��β"]="VAW";
	station["����"]="MYW";
	station["÷��"]="MOQ";
	station["������"]="MLX";
	station["������"]="NVH";
	station["�ϲ�"]="NCB";
	station["�ϳ�"]="NCW";
	station["�ϵ�"]="NDZ";
	station["�ϴ���"]="NMP";
	station["�Ϸ�"]="NFT";
	station["ګ��"]="NHX";
	station["�۽�"]="NGX";
	station["�ڽ�"]="NJW";
	station["��ƽ"]="NPS";
	station["��ͨ"]="NUH";
	station["����"]="NFF";
	station["����ɽ"]="NZX";
	station["ƽ��ɽ"]="PEN";
	station["�̽�"]="PVD";
	station["ƽ��"]="PIJ";
	station["ƽ����"]="POJ";
	station["ƽȪ"]="PQP";
	station["ƺʯ"]="PSQ";
	station["Ƽ��"]="PXG";
	station["ƾ��"]="PXZ";
	station["ۯ����"]="PCW";
	station["��֦��"]="PRW";
	station["ޭ��"]="QRN";
	station["���ɽ"]="QSW";
	station["�ൺ"]="QDK";
	station["��ӳ�"]="QYP";
	station["ǭ��"]="QNW";
	station["����"]="QJM";
	station["ǰ����"]="QEB";
	station["�������"]="QHX";
	station["��̨��"]="QTB";
	station["����"]="QVV";
	station["Ȫ�ݶ�"]="QRS";
	station["Ȫ��"]="QYS";
	station["����"]="QEH";
	station["�ڰ�"]="RAZ";
	station["�����"]="RQJ";
	station["���"]="RJG";
	station["����"]="RZK";
	station["˫�Ǳ�"]="SCB";
	station["��Һ�"]="SFB";
	station["�عض�"]="SGQ";
	station["ɽ����"]="SHD";
	station["�绯"]="SHB";
	station["���䷿"]="SFX";
	station["�ռ���"]="SXT";
	station["����"]="SLL";
	station["����"]="SMS";
	station["��ľ"]="OMY";
	station["����Ͽ"]="SMF";
	station["����"]="ONY";
	station["����"]="NIW";
	station["��ƽ"]="SPT";
	station["����"]="SQF";
	station["����"]="SRG";
	station["��ɽ"]="SSQ";
	station["����"]="OAH";
	station["��ͷ"]="OTQ";
	station["����"]="SWS";
	station["����"]="OEP";
	station["����"]="SEQ";
	station["����"]="SYQ";
	station["ʮ��"]="SNN";
	station["˫Ѽɽ"]="SSB";
	station["��ԭ"]="VYT";
	station["����"]="SZQ";
	station["����"]="SZH";
	station["����"]="SZN";
	station["����"]="OXH";
	station["˷��"]="SUV";
	station["������"]="OSQ";
	station["����"]="TBQ";
	station["������"]="TVX";
	station["����"]="TGY";
	station["����"]="TGP";
	station["����"]="TXX";
	station["ͨ��"]="THL";
	station["̩��"]="TLX";
	station["��³��"]="TFR";
	station["ͨ��"]="TLD";
	station["����"]="TLT";
	station["������"]="TPT";
	station["ͼ��"]="TML";
	station["ͭ��"]="RDQ";
	station["��ɽ��"]="FUP";
	station["��ʦ��"]="TFT";
	station["̩ɽ"]="TAK";
	station["��ˮ"]="TSJ";
	station["��ɽ"]="TSP";
	station["ͨԶ��"]="TYT";
	station["̫����"]="TQT";
	station["̩��"]="UTH";
	station["ͩ��"]="TZW";
	station["ͨ����"]="TAP";
	station["�峣"]="WCB";
	station["���"]="WCN";
	station["�߷���"]="WDT";
	station["����"]="WKK";
	station["�ߺ�"]="WHH";
	station["�ں���"]="WXC";
	station["�����"]="WJT";
	station["��¡"]="WLW";
	station["��������"]="WWT";
	station["μ��"]="WNY";
	station["����"]="WSM";
	station["��ͷɽ"]="WIT";
	station["����"]="WUJ";
	station["������"]="WWJ";
	station["����"]="WXH";
	station["����"]="WXR";
	station["������"]="WPB";
	station["����ɽ"]="WAS";
	station["��Դ"]="WYY";
	station["����"]="WYW";
	station["����"]="WZZ";
	station["����"]="RZH";
	station["������"]="VRH";
	station["����"]="ECW";
	station["���"]="XCF";
	station["������"]="ENW";
	station["�㷻"]="XFB";
	station["����"]="XGV";
	station["�˹�"]="EUG";
	station["����"]="XHY";
	station["�»�"]="EFQ";
	station["�»�"]="XLQ";
	station["���ֺ���"]="XTC";
	station["��¡��"]="EXP";
	station["���ű�"]="XKS";
	station["����"]="XMS";
	station["���Ÿ���"]="XBS";
	station["��ɽ"]="ETW";
	station["С��"]="XST";
	station["����"]="XTG";
	station["����"]="XWM";
	station["����"]="XXF";
	station["����"]="XUN";
	station["����"]="XYY";
	station["����"]="XFN";
	station["������"]="XYT";
	station["����"]="XRZ";
	station["����"]="VIH";
	station["����"]="XUG";
	station["����"]="XCH";
	station["�Ӱ�"]="YWY";
	station["�˱�"]="YBW";
	station["�ǲ�����"]="YWB";
	station["Ҷ����"]="YBD";
	station["�˲���"]="HAN";
	station["����"]="YCW";
	station["�˴�"]="YCG";
	station["�˲�"]="YCN";
	station["�γ�"]="AFH";
	station["�˳�"]="YNV";
	station["����"]="YCB";
	station["�ܴ�"]="YCV";
	station["���"]="YBP";
	station["���"]="YGW";
	station["����"]="YIV";
	station["�Ӽ�"]="YJL";
	station["Ӫ��"]="YKT";
	station["����ʯ"]="YKX";
	station["����"]="YNY";
	station["����"]="YLZ";
	station["����"]="ALY";
	station["һ����"]="YPB";
	station["����"]="YMR";
	station["��ƽ��"]="YAY";
	station["����"]="YZW";
	station["ԭƽ"]="YPV";
	station["����"]="YNP";
	station["��Ȫ��"]="YYV";
	station["��Ȫ"]="YQB";
	station["��Ȫ"]="AQP";
	station["��ɽ"]="YNG";
	station["Ӫɽ"]="NUW";
	station["��ɽ"]="AOP";
	station["����"]="YRT";
	station["ӥ̶"]="YTG";
	station["��̨"]="YAK";
	station["��ͼ���"]="YEX";
	station["������"]="ATP";
	station["����"]="YWH";
	station["����"]="YON";
	station["����"]="YXD";
	station["����"]="AEQ";
	station["����"]="YYQ";
	station["����"]="AOQ";
	station["����"]="YLH";
	station["�Ͳ�"]="ZBK";
	station["��ǵ�"]="ZDV";
	station["�Թ�"]="ZGW";
	station["�麣"]="ZHQ";
	station["�麣��"]="ZIQ";
	station["տ��"]="ZJZ";
	station["��"]="ZJH";
	station["�żҽ�"]="DIQ";
	station["�żҿ�"]="ZKP";
	station["�żҿ���"]="ZMP";
	station["�ܿ�"]="ZKN";
	station["����ľ"]="ZLC";
	station["������"]="ZTX";
	station["פ���"]="ZDN";
	station["����"]="ZVQ";
	station["��ˮ��"]="ZIT";
	station["��ͨ"]="ZDW";
	station["����"]="ZWJ";
	station["����"]="ZYW";
	station["����"]="ZIW";
	station["��ׯ"]="ZEK";
	station["����"]="ZZW";
	station["����"]="ZZQ";
	station["��ׯ��"]="ZFK";
	station["����Ϫ"]="AAX";
	station["����"]="ACB";
	station["����"]="ADX";
	station["����"]="ADP";
	station["����"]="AGT";
	station["����"]="AHP";
	station["����"]="PKQ";
	station["���Ҵ�"]="AJJ";
	station["����"]="ARH";
	station["����"]="AJB";
	station["����"]="AJD";
	station["������"]="AER";
	station["����Ҥ"]="AYY";
	station["��������"]="ALD";
	station["����"]="AUZ";
	station["����ɽ"]="ASX";
	station["��½"]="ALN";
	station["��ľ��"]="JTX";
	station["����ׯ"]="AZM";
	station["������"]="APH";
	station["��ɽ��"]="AXT";
	station["����"]="ATV";
	station["��ͤ��"]="ASH";
	station["��ͼʲ"]="ATR";
	station["��ͼ"]="ATL";
	station["��Ϫ"]="AXS";
	station["����"]="BWQ";
	station["�ױڹ�"]="BGV";
	station["������"]="BMH";
	station["�ͳ�"]="BCR";
	station["���"]="BUP";
	station["������"]="BEP";
	station["����"]="BDP";
	station["����"]="BPP";
	station["�˴���"]="ILP";
	station["�Ͷ�"]="BNN";
	station["�ع�"]="BGM";
	station["����"]="BUT";
	station["�׺Ӷ�"]="BIY";
	station["�ں�"]="BVC";
	station["����ɽ"]="BWH";
	station["�׺���"]="BEY";
	station["��ܸ��"]="BJJ";
	station["�̼���"]="BJM";
	station["����"]="IBQ";
	station["�׼���"]="BBM";
	station["�ʼ�ɽ"]="BSB";
	station["�˽�̨"]="BTD";
	station["����"]="BKD";
	station["�׿���"]="BKB";
	station["����"]="BAT";
	station["����"]="BRZ";
	station["����"]="BOR";
	station["������"]="BQC";
	station["����"]="BLX";
	station["����"]="BNB";
	station["����"]="BOZ";
	station["����"]="BLB";
	station["���п�"]="BLR";
	station["����ɽ"]="BND";
	station["�����"]="BMD";
	station["��è��"]="BNM";
	station["����ͨ"]="BMB";
	station["����Ȧ��"]="BRP";
	station["��Ʊ��"]="RPD";
	station["����"]="BQP";
	station["��Ȫ��"]="BQB";
	station["��ɳ"]="BSW";
	station["��ɽ"]="BAY";
	station["��ˮ��"]="BSY";
	station["��ɳ��"]="BPM";
	station["��ʯɽ"]="BAL";
	station["��ˮ��"]="BUM";
	station["����"]="BTQ";
	station["��ͷ"]="BZP";
	station["����"]="BYP";
	station["����"]="BXK";
	station["����Ͳ"]="VXD";
	station["�������"]="BYC";
	station["�����"]="BYB";
	station["��Ӫ"]="BIV";
	station["�������"]="BAC";
	station["��������"]="BID";
	station["����Ȧ"]="BYT";
	station["������"]="BNJ";
	station["������˶"]="BCD";
	station["����"]="IEW";
	station["����"]="RMP";
	station["��լ"]="BVP";
	station["��ڱ�"]="CIN";
	station["�鲼��"]="CBC";
	station["����"]="CEJ";
	station["����"]="CCM";
	station["�е¶�"]="CCP";
	station["�����"]="CID";
	station["�ϸ�"]="CAX";
	station["����"]="CEF";
	station["�񹵱�"]="CGV";
	station["�ǹ�"]="CGY";
	station["�¹�Ӫ"]="CAJ";
	station["�ɸ���"]="CZB";
	station["�ݺ�"]="WBW";
	station["���"]="CHB";
	station["���"]="CHZ";
	station["�ݺӿ�"]="CKT";
	station["�޻ƿ�"]="CHP";
	station["����"]="CIH";
	station["�̼ҹ�"]="CJT";
	station["�ɼ�˼��"]="CJX";
	station["��"]="CAM";
	station["�̼���"]="CJY";
	station["�׿�"]="CKK";
	station["����"]="CLK";
	station["������"]="CYP";
	station["����"]="CUQ";
	station["����"]="CLP";
	station["������"]="CLT";
	station["����"]="CMB";
	station["��ũ"]="CNJ";
	station["��ƽ��"]="VBP";
	station["������"]="CPM";
	station["����"]="CQB";
	station["��ɽ"]="CSB";
	station["����"]="EFW";
	station["��ɽ"]="CSP";
	station["��ʯ"]="CST";
	station["����"]="CSL";
	station["������"]="CSC";
	station["��ɽ��"]="CVT";
	station["��͡"]="CES";
	station["��ͼ��"]="CPT";
	station["����"]="CQQ";
	station["����"]="CIP";
	station["�Ϫ"]="CNZ";
	station["��Ϫ"]="CXQ";
	station["����"]="CRP";
	station["������"]="CFH";
	station["��Ҥ"]="CYK";
	station["����"]="CYD";
	station["����"]="CAL";
	station["����"]="CEK";
	station["��ҵ��"]="CEX";
	station["������"]="CYL";
	station["������"]="CDD";
	station["��ԫ"]="CYF";
	station["������"]="CZL";
	station["���ݱ�"]="CUH";
	station["���ݱ�"]="ESH";
	station["����"]="CXH";
	station["����"]="CKQ";
	station["��ׯ"]="CVK";
	station["������"]="CFP";
	station["��ת��"]="CWM";
	station["������"]="ICQ";
	station["������"]="CBP";
	station["�°�"]="DAG";
	station["����"]="DAZ";
	station["���"]="DBJ";
	station["���"]="DBC";
	station["���"]="DBD";
	station["����"]="RBT";
	station["����"]="DYJ";
	station["���߾�"]="DBB";
	station["�²�˹"]="RDT";
	station["���"]="DGJ";
	station["�²�"]="DVW";
	station["�ε�"]="DDB";
	station["���"]="DEM";
	station["���㹵"]="DKJ";
	station["������"]="DRD";
	station["�ö�����"]="DRX";
	station["����"]="UFQ";
	station["����"]="DGY";
	station["����"]="DIL";
	station["����"]="DMM";
	station["�����"]="DTT";
	station["���"]="RGW";
	station["����"]="DGP";
	station["��ݸ"]="DAQ";
	station["����"]="DHB";
	station["��ҳ�"]="DHP";
	station["�����"]="DQD";
	station["������"]="DQH";
	station["�»���"]="DXT";
	station["��ҹ�"]="DJT";
	station["����"]="DKB";
	station["�ż�"]="DJL";
	station["���ׯ"]="DJM";
	station["�����"]="DKP";
	station["����"]="RVD";
	station["�����"]="DHO";
	station["��½��"]="DLC";
	station["����"]="DLB";
	station["����"]="DLD";
	station["��������"]="DIC";
	station["������"]="DTX";
	station["��������"]="DNC";
	station["������"]="DMD";
	station["�����"]="DEP";
	station["������"]="DNF";
	station["����"]="DNZ";
	station["��ƽ��"]="DPD";
	station["����ʯ"]="RPP";
	station["����"]="DPI";
	station["��"]="DVT";
	station["��������"]="DQX";
	station["����"]="DML";
	station["����ɽ"]="DQB";
	station["������"]="MOH";
	station["����"]="DRQ";
	station["��ɽ"]="RWW";
	station["�ɽ"]="DKH";
	station["��ɳ��"]="DWT";
	station["������"]="DPM";
	station["��ʯͷ"]="DSL";
	station["��ʯկ"]="RZT";
	station["��̨"]="DBH";
	station["����"]="DQK";
	station["����"]="DGT";
	station["�����"]="DBM";
	station["��ͨ��"]="DTL";
	station["��ͽ"]="RUH";
	station["����"]="DNT";
	station["����"]="DRJ";
	station["�����"]="DFJ";
	station["������"]="DWJ";
	station["����̲"]="DZZ";
	station["������"]="DFM";
	station["���˹�"]="DXL";
	station["����"]="DXX";
	station["����"]="DSJ";
	station["����"]="DXM";
	station["����"]="DXG";
	station["����"]="DKV";
	station["����"]="DXV";
	station["����"]="RXP";
	station["����ׯ"]="DXD";
	station["����"]="DYH";
	station["����"]="DYX";
	station["����"]="DYW";
	station["����"]="DYN";
	station["������"]="EXH";
	station["��Ӣ��"]="IAW";
	station["���ٵ�"]="DBV";
	station["��Ӫ"]="DYV";
	station["��Զ"]="EWH";
	station["���"]="RYV";
	station["��Ԫ"]="DYZ";
	station["��Ӫ��"]="DJP";
	station["��Ӫ��"]="DZD";
	station["��ս��"]="DTJ";
	station["���ݶ�"]="DIP";
	station["��ׯ"]="DVQ";
	station["����"]="DNV";
	station["����"]="DFZ";
	station["����"]="DCH";
	station["����"]="DWV";
	station["��ׯ"]="ROP";
	station["����"]="DXP";
	station["����԰"]="DZY";
	station["������"]="DAP";
	station["����ׯ"]="RZP";
	station["���"]="EBW";
	station["��������"]="RDP";
	station["������"]="RDX";
	station["����"]="RLD";
	station["����ɽ��"]="ELA";
	station["��ü"]="EMW";
	station["��Ӫ"]="RYJ";
	station["����"]="ECN";
	station["����"]="FAS";
	station["����"]="FAZ";
	station["���"]="FCG";
	station["�����"]="FNG";
	station["�ʶ�"]="FIH";
	station["����"]="FEM";
	station["����"]="FHX";
	station["����"]="FHR";
	station["��˳�"]="FHT";
	station["�"]="FHH";
	station["����"]="FIB";
	station["������"]="FTT";
	station["������"]="FTB";
	station["������"]="FZB";
	station["����"]="FNH";
	station["����"]="AKH";
	station["����"]="FNP";
	station["����"]="FQS";
	station["��Ȫ"]="VMW";
	station["��ˮ��"]="FSJ";
	station["��˳"]="FUQ";
	station["����"]="FSV";
	station["��˳"]="FST";
	station["��ɽ��"]="FKP";
	station["����"]="FSZ";
	station["����"]="FTX";
	station["��ͼ��"]="FYP";
	station["���ض�"]="FDY";
	station["����"]="FXY";
	station["����"]="FEY";
	station["����"]="FXK";
	station["����"]="FUH";
	station["����"]="FAV";
	station["���౱"]="FBT";
	station["����"]="FYG";
	station["��Դ"]="FYM";
	station["����"]="FYT";
	station["��ԣ"]="FYX";
	station["���ݱ�"]="FBG";
	station["����"]="FZY";
	station["����"]="FZC";
	station["����"]="VZK";
	station["�̰�"]="GFP";
	station["�㰲"]="VJW";
	station["�߱���"]="GBP";
	station["������"]="GBD";
	station["�ʲݵ�"]="GDJ";
	station["�ȳ�"]="GCN";
	station["޻��"]="GEP";
	station["�ų���"]="GZB";
	station["���"]="GRH";
	station["��"]="GTW";
	station["����"]="IDW";
	station["�Ŷ�"]="GDV";
	station["���"]="GGZ";
	station["�ٸ�"]="GVP";
	station["�����"]="GGT";
	station["�ʹ�"]="GGJ";
	station["�߸�ׯ"]="GGP";
	station["�ʺ�"]="GAX";
	station["����"]="GEX";
	station["���ҵ�"]="GDT";
	station["�¼���"]="GKT";
	station["����"]="GOB";
	station["����"]="GLJ";
	station["����"]="GEJ";
	station["��¥��"]="GFM";
	station["������"]="GHT";
	station["����"]="GLF";
	station["����"]="VOW";
	station["����ׯ"]="GLP";
	station["����"]="GMK";
	station["������"]="GMC";
	station["��ũ��"]="GRT";
	station["������"]="GNT";
	station["������"]="GNM";
	station["��ƽ"]="GPF";
	station["��Ȫ��"]="GEY";
	station["�����"]="GAG";
	station["���쿨"]="GQD";
	station["��Ȫ"]="GQY";
	station["������"]="GZD";
	station["��ˮ"]="GSW";
	station["��ˮ"]="GST";
	station["��ɽ��"]="GSP";
	station["����"]="GSL";
	station["��ɽ��"]="GSD";
	station["��ʲ����"]="GXD";
	station["��̨"]="GTJ";
	station["��̲"]="GAY";
	station["����"]="GTS";
	station["����"]="GTP";
	station["��ͨ"]="GOM";
	station["������"]="KEP";
	station["��Ϫ"]="GXG";
	station["����"]="GYH";
	station["����"]="GXF";
	station["����"]="GIP";
	station["������"]="GYF";
	station["��ԭ"]="GUJ";
	station["��԰"]="GYL";
	station["��Ӫ��"]="GYD";
	station["����"]="GZS";
	station["����"]="GNQ";
	station["����"]="GZJ";
	station["����"]="GSQ";
	station["����"]="GEH";
	station["����"]="GXT";
	station["���־�"]="GOT";
	station["����"]="GZT";
	station["����ɽ"]="GSS";
	station["������"]="GAT";
	station["�찲"]="HWN";
	station["������"]="AMH";
	station["�찲��"]="VXN";
	station["������"]="HIH";
	station["�ư�"]="HBL";
	station["����"]="HEB";
	station["�ױ�"]="HAF";
	station["����"]="VCQ";
	station["�ϴ�"]="WKW";
	station["�Ӵ�"]="HCZ";
	station["����"]="HCN";
	station["����"]="HCT";
	station["�ڳ�̲"]="HCJ";
	station["�ƴ�"]="HCP";
	station["������"]="HXT";
	station["����"]="HGC";
	station["�鶴"]="HDV";
	station["���"]="HFG";
	station["������"]="HXJ";
	station["����"]="HGP";
	station["�ƹ�԰"]="HYM";
	station["�����"]="IGW";
	station["�컨��"]="VHD";
	station["�ƻ�Ͳ"]="HUD";
	station["�ؼҵ�"]="HJJ";
	station["�;�"]="HJR";
	station["�콭"]="HFM";
	station["�ھ�"]="HIM";
	station["���"]="HJF";
	station["�ӽ�"]="HJV";
	station["����"]="HJS";
	station["�Ӽ���"]="HXP";
	station["����ׯ"]="HJM";
	station["�ӿ���"]="HKJ";
	station["�ƿ�"]="KOH";
	station["����"]="HKG";
	station["����"]="HUB";
	station["��«����"]="HPD";
	station["������"]="HHB";
	station["������"]="HIT";
	station["����"]="HOB";
	station["����"]="HIB";
	station["����"]="ULY";
	station["����"]="HRB";
	station["����"]="VLB";
	station["����"]="HAT";
	station["����"]="HLL";
	station["����"]="HIL";
	station["������"]="HAX";
	station["��³˹̫"]="VTJ";
	station["��÷"]="VEH";
	station["�����"]="HMT";
	station["����Ӫ"]="HYP";
	station["�����"]="HHL";
	station["����"]="HNH";
	station["��ũ"]="HMJ";
	station["��ƽ"]="VAQ";
	station["������"]="HZM";
	station["����"]="VQH";
	station["����"]="HEY";
	station["����"]="HRV";
	station["����"]="HRN";
	station["��ɽ��"]="HDY";
	station["���ɵ�"]="HDL";
	station["��ʲ�����"]="VSR";
	station["��ɽ"]="VSB";
	station["����"]="VSQ";
	station["��ɽ"]="HSQ";
	station["��ˮ"]="HOT";
	station["��ɽ"]="VCH";
	station["��ʲ��"]="HHP";
	station["���±�"]="HSJ";
	station["��ʯ��"]="HSO";
	station["��ɽ��"]="HEQ";
	station["��ɰ�"]="VSJ";
	station["��̨"]="HQB";
	station["��̨"]="VTK";
	station["����"]="VTR";
	station["��ͬ"]="VTQ";
	station["������"]="HZT";
	station["����"]="HWK";
	station["����"]="RWH";
	station["����"]="VXB";
	station["����"]="HYY";
	station["����¡"]="VHB";
	station["������"]="VTB";
	station["���̨"]="HTJ";
	station["����"]="VIX";
	station["����"]="HAY";
	station["����"]="HYK";
	station["������"]="HVQ";
	station["����"]="HUW";
	station["����"]="HQY";
	station["����̲"]="HGJ";
	station["��Դ"]="WHW";
	station["��Դ"]="VIQ";
	station["��԰"]="HUN";
	station["������"]="HYJ";
	station["����"]="HZZ";
	station["����"]="VON";
	station["����"]="HZV";
	station["������"]="VXQ";
	station["�ޱ�"]="JRT";
	station["����"]="JIY";
	station["����"]="JBD";
	station["���Ǳ�"]="JEF";
	station["���"]="JCJ";
	station["۲��"]="JCK";
	station["����"]="JNV";
	station["����"]="JFD";
	station["����"]="JDB";
	station["����"]="JFP";
	station["����"]="JOB";
	station["����"]="UDH";
	station["����ɽ"]="JST";
	station["����"]="VGP";
	station["����"]="JHP";
	station["���"]="JHX";
	station["����"]="JHB";
	station["����"]="JHQ";
	station["����"]="JHR";
	station["������"]="JIR";
	station["����"]="JHZ";
	station["����"]="AJH";
	station["�ͼҹ�"]="VJD";
	station["����"]="JJS";
	station["����"]="JJW";
	station["����"]="JJB";
	station["���"]="JKT";
	station["ܸ��"]="JLJ";
	station["�����"]="JMM";
	station["����"]="JES";
	station["����"]="JWQ";
	station["����"]="JOK";
	station["����"]="JNP";
	station["���"]="JVS";
	station["����"]="JPC";
	station["����"]="JQX";
	station["����"]="SSX";
	station["��ɽ��"]="EGH";
	station["��ɽ"]="JCN";
	station["��ʼ"]="JRN";
	station["����"]="JSH";
	station["�ɽ"]="JVV";
	station["����"]="JSL";
	station["����"]="JET";
	station["��ɽ"]="JOP";
	station["������"]="JIB";
	station["������"]="EAH";
	station["��ɽ��"]="JTB";
	station["������"]="JOM";
	station["��̩"]="JTJ";
	station["����"]="JWX";
	station["����"]="JUG";
	station["����"]="JKK";
	station["����"]="JUK";
	station["����"]="JXV";
	station["����"]="JJP";
	station["����"]="JXH";
	station["������"]="EPH";
	station["������"]="JXT";
	station["����"]="JYW";
	station["����"]="JRQ";
	station["����"]="JYS";
	station["����"]="UEH";
	station["��Ұ"]="JYK";
	station["����"]="JYZ";
	station["��Զ"]="JYJ";
	station["����"]="JYH";
	station["��Դ"]="SZL";
	station["��Դ"]="JYF";
	station["��Զ��"]="JXJ";
	station["���ݱ�"]="JZK";
	station["������"]="WEF";
	station["����"]="JEQ";
	station["����"]="JBN";
	station["��կ"]="JZH";
	station["����"]="JXP";
	station["����"]="JXK";
	station["������"]="JOD";
	station["����"]="JOF";
	station["��ׯ��"]="JVP";
	station["������"]="JYD";
	station["����"]="KAT";
	station["�⳵"]="KCR";
	station["����"]="KCP";
	station["�ⶼ��"]="KDX";
	station["���"]="KDT";
	station["�˶�"]="KOB";
	station["����"]="KAW";
	station["����"]="KJB";
	station["������"]="KQX";
	station["��³"]="KLC";
	station["��������"]="KHR";
	station["��ǰ"]="KQL";
	station["��ɽ"]="KAB";
	station["��ɽ"]="KSH";
	station["��ɽ"]="KSB";
	station["��ͨ"]="KTT";
	station["������"]="KXZ";
	station["��һ��"]="KHX";
	station["��ԭ��"]="KXT";
	station["��ׯ"]="KZP";
	station["����"]="UBZ";
	station["�ϱ�"]="LLT";
	station["�鱦��"]="LPF";
	station["����"]="LUQ";
	station["�ֲ�"]="LCQ";
	station["���"]="UCP";
	station["�ĳ�"]="UCK";
	station["����"]="LCK";
	station["�ֶ�"]="LRC";
	station["�ֶ�"]="LDO";
	station["������"]="LDP";
	station["��������"]="LVP";
	station["³��"]="LVM";
	station["�ȷ�"]="LJP";
	station["����"]="LOP";
	station["�ȷ���"]="LFP";
	station["����"]="LFX";
	station["»��"]="LFM";
	station["�ϸ�"]="UFD";
	station["����"]="LNB";
	station["���ǵ�"]="LGM";
	station["«��"]="LOM";
	station["����"]="LGJ";
	station["����"]="LGB";
	station["�ٺ�"]="UFH";
	station["�ֺ�"]="LXX";
	station["����"]="LHX";
	station["�躣"]="JID";
	station["����"]="LNL";
	station["����"]="KLH";
	station["����"]="LHP";
	station["�к���"]="UNP";
	station["������"]="LEX";
	station["���׵�"]="LRT";
	station["���ҵ�"]="UDT";
	station["���Һ�"]="LVT";
	station["����"]="LKS";
	station["���"]="LJB";
	station["�޽�"]="LJW";
	station["����"]="LJZ";
	station["®��"]="UJH";
	station["����"]="LID";
	station["����"]="UJT";
	station["����"]="LJX";
	station["����"]="UJL";
	station["������"]="LHB";
	station["����¥"]="ULK";
	station["���ƺ"]="LIJ";
	station["����"]="LKF";
	station["�ֿ�"]="LKB";
	station["·����"]="LKQ";
	station["����"]="LAX";
	station["����"]="LAB";
	station["½��"]="LRM";
	station["����"]="LLW";
	station["����"]="UWZ";
	station["���"]="LWQ";
	station["����"]="LLB";
	station["¬��"]="UAP";
	station["�����"]="LMX";
	station["��ľ��"]="LMB";
	station["����"]="LMJ";
	station["���ź�"]="MHA";
	station["����"]="UNG";
	station["��ƽ"]="UQW";
	station["��ƽ"]="LPM";
	station["������"]="LPP";
	station["����ɽ"]="UPJ";
	station["��ƽ��"]="LPG";
	station["����"]="UQK";
	station["��Ȫ��"]="UQJ";
	station["���ƴ�"]="LUM";
	station["��ˮ����"]="UDQ";
	station["��ɽ��"]="LGT";
	station["��ˮ��"]="USP";
	station["��ˮ"]="LIQ";
	station["��ɽ"]="UTW";
	station["��ɽ"]="LRN";
	station["³ɽ"]="LAF";
	station["��ˮ"]="USH";
	station["��ɽ"]="LMK";
	station["��ʯ"]="LSV";
	station["¶ˮ��"]="LUL";
	station["®ɽ"]="LSG";
	station["��ʢ��"]="LBT";
	station["������"]="LSD";
	station["������"]="LSB";
	station["��ɽ��"]="LAS";
	station["��ʯկ"]="LET";
	station["����"]="LTZ";
	station["��̨"]="LAR";
	station["«̨"]="LTP";
	station["������"]="LBM";
	station["����"]="LVZ";
	station["������"]="LTJ";
	station["����"]="VLJ";
	station["���߶�"]="LWK";
	station["��βɽ"]="LRJ";
	station["����"]="LNJ";
	station["������"]="UXK";
	station["����"]="LXB";
	station["¤��"]="LXY";
	station["����"]="LXQ";
	station["����"]="LXK";
	station["����"]="LXC";
	station["����"]="UXP";
	station["����"]="LYY";
	station["����"]="LYK";
	station["����"]="LYT";
	station["���ʱ�"]="UYK";
	station["��Դ��"]="LDD";
	station["���Ƹ�"]="UIH";
	station["����"]="LYC";
	station["���"]="LNF";
	station["��Ӫ"]="LXL";
	station["����"]="LMH";
	station["��Դ"]="LVS";
	station["��Դ"]="LYX";
	station["��Դ"]="LAQ";
	station["�Դ"]="LYP";
	station["������"]="LPQ";
	station["����"]="LEJ";
	station["��צ��"]="LZT";
	station["����"]="UAQ";
	station["��֦"]="LIW";
	station["¹կ"]="LIZ";
	station["����"]="LZS";
	station["����"]="LZA";
	station["����"]="LEM";
	station["����"]="MAC";
	station["��ɽ"]="MAH";
	station["ë��"]="MBY";
	station["ë�ӹ�"]="MGY";
	station["��Ǳ�"]="MBN";
	station["�ų�"]="MCF";
	station["����"]="MCL";
	station["���"]="MAP";
	station["�ų���"]="MNF";
	station["é��ƺ"]="KPM";
	station["�Ͷ���"]="MUQ";
	station["ĥ��ʯ"]="MOB";
	station["�ֶ�"]="MDF";
	station["ñ��ɽ"]="MRB";
	station["����"]="MGN";
	station["÷�ӿ�"]="MHL";
	station["���"]="MHZ";
	station["�ϼҸ�"]="MGB";
	station["����"]="MHQ";
	station["���޶�"]="MQQ";
	station["������"]="MHB";
	station["é��"]="MLZ";
	station["����"]="MLL";
	station["����"]="MLB";
	station["����"]="MID";
	station["����"]="MGM";
	station["����"]="MLQ";
	station["ľ��ͼ"]="MUD";
	station["������"]="MMM";
	station["����˹��"]="MNR";
	station["����"]="UGW";
	station["����"]="MPQ";
	station["���ź�"]="MQB";
	station["����"]="MQS";
	station["��Ȩ"]="MQF";
	station["��ˮ��"]="MUT";
	station["��ɽ"]="MAB";
	station["üɽ"]="MSW";
	station["��ˮ��"]="MKW";
	station["ï����"]="MOM";
	station["��ɳ��"]="MST";
	station["��̨��"]="MZB";
	station["��Ϫ"]="MEB";
	station["����"]="MVY";
	station["����"]="MVQ";
	station["�����"]="MCM";
	station["����"]="MMW";
	station["��԰"]="MYS";
	station["ī��"]="MUR";
	station["����"]="MUP";
	station["��ׯ"]="MZJ";
	station["��֬"]="MEY";
	station["����"]="MFQ";
	station["����"]="NAB";
	station["ũ��"]="NAT";
	station["�ϲ�ɽ"]="NBK";
	station["�ϳ�"]="NCK";
	station["�ϳ�˾"]="NSP";
	station["����"]="NCZ";
	station["����"]="NES";
	station["�Ϲ۴�"]="NGP";
	station["�Ϲ���"]="NFP";
	station["�Ϲ���"]="NLT";
	station["����"]="NNH";
	station["����"]="NHH";
	station["�ϺӴ�"]="NHJ";
	station["�ϻ�"]="NHS";
	station["����"]="NVT";
	station["ţ��"]="NJB";
	station["�Ͼ�"]="NJS";
	station["�ܼ�"]="NJD";
	station["�Ͽ�"]="NKP";
	station["�Ͽ�ǰ"]="NKT";
	station["����"]="NNQ";
	station["����"]="NLD";
	station["���տ�"]="NIR";
	station["����"]="ULZ";
	station["������"]="NLF";
	station["����"]="NMD";
	station["����"]="NMZ";
	station["��ľ"]="NMX";
	station["��ƽ��"]="NNS";
	station["����"]="NPZ";
	station["����"]="NQD";
	station["����"]="NQO";
	station["ůȪ"]="NQJ";
	station["��̨"]="NTT";
	station["��ͷ"]="NOQ";
	station["����"]="NWV";
	station["������"]="NWP";
	station["���豱"]="NEH";
	station["����"]="NXQ";
	station["����"]="NXF";
	station["ţ��̨"]="NXT";
	station["����"]="NUP";
	station["���ӹ�"]="NIP";
	station["����"]="NAF";
	station["����ľ"]="NZT";
	station["ƽ��"]="PAL";
	station["�"]="PAW";
	station["ƽ����"]="PNO";
	station["�Ͱ���"]="PAJ";
	station["ƽ����"]="PZT";
	station["�ѳǶ�"]="PEY";
	station["�ѳ�"]="PCY";
	station["���"]="PDB";
	station["ƫ��"]="PRP";
	station["ƽ��ɽ��"]="BFF";
	station["�µ���"]="PXJ";
	station["ư����"]="PRT";
	station["ƽ��"]="PFB";
	station["ƽ��"]="PGM";
	station["�̹�"]="PAM";
	station["ƽ��"]="PGZ";
	station["�ǻ���"]="PHP";
	station["ƽ�ӿ�"]="PHM";
	station["�̽���"]="PBD";
	station["�˼ҵ�"]="PDP";
	station["Ƥ��"]="PKT";
	station["������"]="PLT";
	station["ƫ��"]="PNT";
	station["ƽɽ"]="PSB";
	station["��ɽ"]="PSW";
	station["Ƥɽ"]="PSR";
	station["��ˮ"]="PHW";
	station["��ʯ"]="PSL";
	station["ƽ��"]="PSV";
	station["ƽ̨"]="PVT";
	station["ƽ��"]="PTM";
	station["����"]="PTS";
	station["����ݼ"]="PTW";
	station["ƽ��"]="PWV";
	station["����"]="PWT";
	station["����"]="POW";
	station["ƽ��"]="PYX";
	station["����"]="PYJ";
	station["ƽң"]="PYV";
	station["ƽ��"]="PIK";
	station["ƽԭ��"]="PPJ";
	station["ƽԭ"]="PYK";
	station["ƽ��"]="PYP";
	station["����"]="PZG";
	station["����"]="PJH";
	station["ƽׯ"]="PZD";
	station["����"]="POD";
	station["ƽׯ��"]="PND";
	station["Ǭ��"]="QOT";
	station["�찲"]="QAB";
	station["Ǩ��"]="QQP";
	station["���"]="QRQ";
	station["�ߵ�"]="QDM";
	station["������"]="QAK";
	station["���"]="QFT";
	station["�����"]="QVP";
	station["����"]="QFK";
	station["�ڷ�Ӫ"]="QFM";
	station["��"]="QYQ";
	station["�ػʵ�"]="QTP";
	station["ǧ��"]="QUY";
	station["���"]="QIP";
	station["�����"]="QHD";
	station["�廪԰"]="QHP";
	station["����"]="QJZ";
	station["�뽭"]="QJW";
	station["Ǳ��"]="QJN";
	station["ȫ��"]="INH";
	station["�ؼ�"]="QJB";
	station["��ұ�"]="QBT";
	station["�彧��"]="QNY";
	station["�ؼ�ׯ"]="QZV";
	station["�����"]="QLD";
	station["����"]="QLZ";
	station["����"]="QLY";
	station["����ɽ"]="QGH";
	station["������"]="QSM";
	station["����"]="QIH";
	station["ǰĥͷ"]="QMP";
	station["��ɽ"]="QSB";
	station["ȫʤ"]="QVB";
	station["ȷɽ"]="QSN";
	station["��ˮ"]="QUJ";
	station["ǰɽ"]="QXQ";
	station["������"]="QYH";
	station["����"]="QVH";
	station["��ͷ"]="QAT";
	station["��ͭϿ"]="QTJ";
	station["ǰέ��"]="QWP";
	station["����"]="QRW";
	station["����"]="QXV";
	station["����"]="QXP";
	station["����"]="QXJ";
	station["����"]="QUV";
	station["����Ӫ"]="QXC";
	station["ǧ��"]="QOY";
	station["����"]="QYF";
	station["Ȫ��"]="QYL";
	station["������"]="QVQ";
	station["��Ӫ"]="QYJ";
	station["����ɽ"]="QSJ";
	station["��Զ"]="QBQ";
	station["��ԭ"]="QYT";
	station["���ݶ�"]="QDZ";
	station["ȫ��"]="QZZ";
	station["����"]="QRZ";
	station["������"]="QZK";
	station["��"]="RAH";
	station["�ٲ�"]="RCW";
	station["���"]="RCG";
	station["���"]="RBH";
	station["�ݹ�"]="RUQ";
	station["����"]="RQP";
	station["��ɽ"]="ROK";
	station["��ˮ"]="RSZ";
	station["��ˮ"]="RSD";
	station["����"]="RXZ";
	station["����"]="RVP";
	station["����"]="RYF";
	station["������"]="RHD";
	station["����"]="ROF";
	station["ʯ��"]="OBJ";
	station["�ϰ��"]="SBP";
	station["ʩ��"]="AQW";
	station["�ϰ����"]="OBP";
	station["˫�Ǳ�"]="SBB";
	station["�̳�"]="SWN";
	station["ɯ��"]="SCR";
	station["˳��"]="SCS";
	station["���"]="OCH";
	station["���"]="SMV";
	station["ɳ��"]="SCP";
	station["ʯ��"]="SCT";
	station["ɽ����"]="SCL";
	station["ɽ��"]="SDJ";
	station["˳��"]="ORQ";
	station["���"]="ODY";
	station["�۶�"]="SOQ";
	station["ˮ��"]="SIL";
	station["�̶�"]="SXC";
	station["ʮ��"]="SEP";
	station["�ĵ���"]="OUD";
	station["��"]="OLH";
	station["˫��"]="OFB";
	station["�ķ�̨"]="STB";
	station["ˮ��"]="OTW";
	station["���ؿ�"]="OKJ";
	station["ɣ������"]="OGC";
	station["�ع�"]="SNQ";
	station["�ϸ���"]="SVK";
	station["�Ϻ�"]="JBS";
	station["ɳ��"]="SED";
	station["�ɺ�"]="SBM";
	station["ɳ��"]="SHP";
	station["ɳ�ӿ�"]="SKT";
	station["��������"]="SHC";
	station["ɳ����"]="VOP";
	station["ɽ����"]="SHL";
	station["������"]="OXP";
	station["�ĺ���"]="OHD";
	station["������"]="OZW";
	station["˫����"]="SEL";
	station["ʯ����"]="SZR";
	station["����ׯ"]="SVP";
	station["���ҵ�"]="ODP";
	station["ˮ�Һ�"]="SQH";
	station["��Һ�"]="OJJ";
	station["�ɽ���"]="SJL";
	station["�м�"]="SJB";
	station["���"]="SUB";
	station["���"]="OJB";
	station["�ɽ�"]="SAH";
	station["������"]="SKD";
	station["˾����"]="OLK";
	station["�ɽ���"]="IMH";
	station["ʯ��ɽ��"]="SRP";
	station["�ۼ���"]="SJJ";
	station["������"]="SOZ";
	station["����կ"]="SMM";
	station["ʮ����"]="SJD";
	station["�ɽ���"]="OZL";
	station["ʩ����"]="SHM";
	station["���"]="SWT";
	station["ʲ���"]="OMP";
	station["����"]="SUR";
	station["���պ�"]="SHJ";
	station["������"]="VLD";
	station["ʯ��"]="SPB";
	station["����"]="SIB";
	station["ʯ��"]="SOL";
	station["ʯ��"]="SLM";
	station["ʯ����"]="LNM";
	station["ʯ��"]="SLQ";
	station["������"]="SLC";
	station["����"]="SNT";
	station["����"]="OLY";
	station["ɳ����"]="SLP";
	station["����Ͽ��"]="SCF";
	station["������"]="OQH";
	station["ʯ����"]="OMQ";
	station["����Ͽ��"]="SXF";
	station["����"]="SYP";
	station["��"]="SOB";
	station["˫��"]="SBZ";
	station["��ƽ��"]="PPT";
	station["��ƽ"]="SON";
	station["ɳ��ͷ"]="SFJ";
	station["������"]="SPF";
	station["ˮȪ"]="SID";
	station["ʯȪ��"]="SXY";
	station["ʯ����"]="SQT";
	station["ʯ�˳�"]="SRB";
	station["ʯ��"]="SRL";
	station["ɽ��"]="SQB";
	station["����"]="SWB";
	station["۷��"]="SSR";
	station["��ˮ"]="SJQ";
	station["��ˮ"]="OSK";
	station["����"]="SFT";
	station["��ɽ"]="SAT";
	station["��ʮ��"]="SRD";
	station["��ʮ�ﱤ"]="SST";
	station["������"]="SSL";
	station["����"]="MZQ";
	station["��ͼ��"]="SHX";
	station["���ü�"]="SDH";
	station["ʯͷ"]="OTB";
	station["��ͷ"]="SEV";
	station["ɳ��"]="SFM";
	station["����"]="SWP";
	station["����"]="SKB";
	station["ɳ����"]="SXR";
	station["��Ϫ"]="SXZ";
	station["ɳ��"]="SAS";
	station["����"]="SOH";
	station["���"]="OVH";
	station["������"]="SXM";
	station["ʯϿ��"]="SXJ";
	station["����"]="SYB";
	station["����"]="FMH";
	station["����"]="SYV";
	station["ˮ��"]="OYP";
	station["������"]="SYJ";
	station["������"]="SPJ";
	station["��Ӫ"]="OEJ";
	station["˳��"]="SOP";
	station["���微"]="OYD";
	station["��Դ��"]="SYL";
	station["��ԭ"]="SAY";
	station["����"]="BDH";
	station["��԰"]="SUD";
	station["ˮԴ"]="OYJ";
	station["ɣ԰��"]="SAJ";
	station["���б�"]="SND";
	station["���ݱ�"]="OHH";
	station["���ݶ�"]="SRH";
	station["���ڶ�"]="BJQ";
	station["����"]="OZP";
	station["����"]="OZY";
	station["����"]="SZD";
	station["��־"]="SZB";
	station["ʦׯ"]="SNM";
	station["����"]="SIN";
	station["ʦ��"]="SEM";
	station["����԰��"]="KAH";
	station["��������"]="ITH";
	station["ʯ��ɽ"]="SZJ";
	station["̩��"]="TMK";
	station["̨��"]="TID";
	station["ͨ����"]="TAJ";
	station["ͩ��"]="TBF";
	station["ͨ��"]="TBB";
	station["����"]="TCX";
	station["ͩ��"]="TTH";
	station["۰��"]="TZK";
	station["����"]="TCL";
	station["�Ҵ�"]="TCK";
	station["ͨ��"]="TRQ";
	station["�ﶫ"]="TDZ";
	station["���"]="TGL";
	station["��������"]="TGC";
	station["̫��"]="TGV";
	station["����"]="THX";
	station["�ĺ�"]="THM";
	station["�ƺ�"]="THF";
	station["̩��"]="THG";
	station["̫��"]="TKH";
	station["�Ž�"]="TIX";
	station["̷�Ҿ�"]="TNJ";
	station["�ռ���"]="TOT";
	station["�Ƽ���"]="PDQ";
	station["ͳ��ׯ"]="TZP";
	station["̩��"]="TKX";
	station["����ë��"]="TMD";
	station["ͼ���"]="TEX";
	station["ͤ��"]="TIZ";
	station["����"]="TFZ";
	station["ͭ��"]="TJH";
	station["����"]="TLB";
	station["������"]="PXT";
	station["����"]="TMN";
	station["������"]="TNN";
	station["̫��ɽ"]="TLS";
	station["������̨"]="TRC";
	station["������"]="TCJ";
	station["����"]="TVW";
	station["���"]="TVT";
	station["̫ƽ��"]="TIT";
	station["̫ƽ��"]="TEB";
	station["ͼǿ"]="TQX";
	station["̨ǰ"]="TTK";
	station["������"]="TQL";
	station["������"]="TQJ";
	station["��ɽ��"]="TCT";
	station["��ɽ"]="TAB";
	station["��ʯ��"]="TIM";
	station["������"]="THB";
	station["ͬ��"]="TXJ";
	station["��Ϫ"]="TSW";
	station["ͩ��"]="TCH";
	station["����"]="TRZ";
	station["��ӳ"]="TKQ";
	station["����"]="TND";
	station["����"]="TYF";
	station["������"]="TIL";
	station["̫��ɽ"]="TYJ";
	station["��ԭ"]="TYB";
	station["������"]="TYP";
	station["���ݶ�"]="TEK";
	station["̨��"]="TZH";
	station["��ף"]="TZJ";
	station["����"]="TXK";
	station["����"]="TZV";
	station["ͩ����"]="TEW";
	station["����ɽ"]="QWH";
	station["�İ�"]="WBP";
	station["�䰲"]="WAP";
	station["������"]="WVP";
	station["��湵"]="WCT";
	station["�Ĳ�"]="WEQ";
	station["�´�"]="WDB";
	station["�������"]="WRB";
	station["�ĵ�"]="WBK";
	station["�����"]="WDL";
	station["�����"]="WHP";
	station["�ĵ�"]="WNZ";
	station["����"]="WVT";
	station["�䵱ɽ"]="WRN";
	station["����"]="WDP";
	station["�ڶ��캹"]="WHX";
	station["Ϋ��"]="WFK";
	station["����"]="WFB";
	station["����"]="WUT";
	station["�߷�����"]="WXT";
	station["����"]="WGB";
	station["�书"]="WGY";
	station["�幵"]="WGL";
	station["�����"]="WGM";
	station["�ں�"]="WVC";
	station["έ��"]="WHB";
	station["����"]="WHF";
	station["��Ҵ�"]="WCJ";
	station["���"]="WUB";
	station["����"]="WAM";
	station["�缳"]="WJP";
	station["������"]="WJJ";
	station["����"]="WQB";
	station["�����"]="WKT";
	station["������"]="WBT";
	station["��������"]="WLC";
	station["����"]="WEB";
	station["������"]="WVX";
	station["����"]="VHH";
	station["����"]="WLK";
	station["������ǰ��"]="WQC";
	station["����ɽ"]="WSC";
	station["������"]="WLX";
	station["μ�ϱ�"]="WBY";
	station["��ū��"]="WRX";
	station["����"]="WNQ";
	station["����"]="WWG";
	station["μ����"]="WVY";
	station["μ����"]="WNJ";
	station["��Ƥ"]="WPT";
	station["�Ɽ"]="WUY";
	station["����"]="WUP";
	station["����"]="WQL";
	station["����"]="WWP";
	station["��Ȫ"]="WQM";
	station["��ɽ"]="WSJ";
	station["��ˮ"]="WEV";
	station["κ��ׯ"]="WSP";
	station["��ͫ"]="WTP";
	station["��̨ɽ"]="WSV";
	station["����ׯ"]="WZJ";
	station["����"]="WVR";
	station["������"]="WGH";
	station["����"]="WVB";
	station["��ϲ"]="WXV";
	station["����"]="WVV";
	station["��������"]="IFH";
	station["��Ѩ"]="WXN";
	station["����"]="WYZ";
	station["����"]="WYB";
	station["��Ӫ"]="WWB";
	station["����"]="RYH";
	station["��Ҥ��"]="WIM";
	station["��ԭ"]="WYC";
	station["έ�ӹ�"]="WZL";
	station["Τׯ"]="WZY";
	station["��կ"]="WZV";
	station["������"]="WZB";
	station["΢����"]="WQP";
	station["κ����"]="WKD";
	station["�°�"]="EAM";
	station["�˰�"]="XAZ";
	station["�°���"]="XAF";
	station["�±���"]="XAP";
	station["�°��"]="EBP";
	station["������"]="XLP";
	station["����"]="ECH";
	station["�˳�"]="XCD";
	station["С��"]="XEM";
	station["�´�Դ"]="XRX";
	station["�³���"]="XCB";
	station["ϲ��"]="EDW";
	station["С�ý�"]="EJM";
	station["������"]="XMP";
	station["С��"]="XEZ";
	station["С��"]="XOD";
	station["������"]="XPC";
	station["Ϣ��"]="XFW";
	station["�ŷ�"]="EFG";
	station["���"]="XFV";
	station["�¸�"]="EGG";
	station["Т��"]="XGN";
	station["���̳�"]="XUJ";
	station["�Ĺ�Ӫ"]="XGJ";
	station["������"]="NBB";
	station["���"]="XXB";
	station["�º�"]="XIR";
	station["����"]="XWJ";
	station["б�ӽ�"]="EEP";
	station["�»���"]="XAX";
	station["�»�"]="XHB";
	station["�»�"]="EHQ";
	station["����"]="XHP";
	station["�˺���"]="XEC";
	station["С����"]="XYD";
	station["�»�԰"]="XYP";
	station["С����"]="EKY";
	station["���"]="XJB";
	station["Ͽ��"]="EJG";
	station["���"]="XJV";
	station["����"]="ENP";
	station["�½�"]="XJM";
	station["���ֿ�"]="EKM";
	station["�����"]="XJT";
	station["���̨"]="XTJ";
	station["л����"]="XMT";
	station["�˿�"]="EKB";
	station["С�"]="EAQ";
	station["����"]="XNB";
	station["��¡��"]="XDD";
	station["����"]="ELP";
	station["����"]="XPX";
	station["С��"]="XLB";
	station["����"]="XLJ";
	station["����"]="XYB";
	station["����"]="GCT";
	station["����"]="XPH";
	station["������"]="XLD";
	station["С·Ϫ"]="XLM";
	station["��¡��"]="XZB";
	station["������"]="XGT";
	station["����"]="XMD";
	station["����ɽ"]="XMB";
	station["������"]="XAT";
	station["Т��"]="XNV";
	station["������"]="XRN";
	station["����"]="ENQ";
	station["����"]="XNN";
	station["��ƽ"]="XPN";
	station["��ƽ"]="XPY";
	station["��ƺ��"]="XPM";
	station["ϼ��"]="XOS";
	station["����"]="EPQ";
	station["Ϭ��"]="XIW";
	station["����"]="XQB";
	station["����"]="XQD";
	station["��Ȫ��"]="XQJ";
	station["������"]="XRL";
	station["С�¹�"]="ESP";
	station["����"]="XSB";
	station["��ʯ"]="XIZ";
	station["�ˮ"]="XZN";
	station["����"]="XSV";
	station["��ˮ"]="XSP";
	station["С��"]="XAM";
	station["������"]="XOB";
	station["������"]="XDT";
	station["������"]="XSJ";
	station["��̶"]="XTQ";
	station["��̨"]="XTP";
	station["������"]="XAN";
	station["��̨��"]="EIP";
	station["����"]="XJQ";
	station["������"]="EPD";
	station["����"]="XWF";
	station["����"]="XSN";
	station["Ϣ��"]="ENN";
	station["����"]="XQY";
	station["����"]="XXQ";
	station["��Ͽ"]="XIF";
	station["Т��"]="XOV";
	station["С�½�"]="XXM";
	station["������"]="XGQ";
	station["��С��"]="XZC";
	station["С��ׯ"]="XXP";
	station["����"]="XDB";
	station["Ѯ��"]="XUY";
	station["Ѯ����"]="XBY";
	station["������"]="XWN";
	station["��ҵ"]="SNZ";
	station["С���"]="XHM";
	station["����"]="EEQ";
	station["С�¾�"]="XFM";
	station["С����"]="XYX";
	station["����"]="EXM";
	station["��ԫ"]="EIF";
	station["������"]="EJH";
	station["������"]="EYB";
	station["������"]="XZJ";
	station["���ݶ�"]="UUH";
	station["���ʷ�"]="XZX";
	station["����"]="XRP";
	station["����"]="XZT";
	station["����"]="XXV";
	station["ϫ��"]="XZD";
	station["������ľ"]="XRD";
	station["������"]="ERP";
	station["Ҧ��"]="YAC";
	station["����"]="YAX";
	station["����"]="YAS";
	station["������"]="YNB";
	station["��Ӵ�"]="YBM";
	station["�ǲ���"]="YBB";
	station["Ԫ��ɽ"]="YUD";
	station["���"]="YAB";
	station["��ݵ�"]="YKM";
	station["���κ�"]="AIH";
	station["ӭ��"]="YYB";
	station["Ҷ��"]="YER";
	station["�γ�"]="YKJ";
	station["�⴨"]="YYY";
	station["����"]="YQQ";
	station["�˳�"]="YIN";
	station["Ӧ��"]="YHN";
	station["���"]="YCK";
	station["�̳�"]="YEK";
	station["��"]="YED";
	station["����"]="YNF";
	station["����"]="YAL";
	station["۩��"]="YPK";
	station["���"]="YAP";
	station["�Ʋ���"]="ACP";
	station["�ݳ���"]="IXH";
	station["Ӫ����"]="YCT";
	station["����"]="YDJ";
	station["Ӣ��"]="YDQ";
	station["����"]="YDM";
	station["����"]="YGS";
	station["�㵴ɽ"]="YGH";
	station["�ڶ�"]="YDG";
	station["԰��"]="YAJ";
	station["Ӣ����"]="IIQ";
	station["����"]="YFZ";
	station["����Ӫ"]="YYM";
	station["���"]="YRB";
	station["����"]="YOV";
	station["����"]="YIK";
	station["�Ѻ�"]="YOB";
	station["�ຼ"]="EVH";
	station["�غӳ�"]="YHP";
	station["�һ�"]="AEP";
	station["��ʺ�"]="YHM";
	station["����"]="URH";
	station["Ӫ��"]="YAM";
	station["�ν�"]="AEW";
	station["�཭"]="YHG";
	station["Ҷ��"]="YCH";
	station["�ོ"]="AJP";
	station["Ҧ��"]="YAT";
	station["���Ҿ�"]="YGJ";
	station["һ�䱤"]="YJT";
	station["Ӣ��ɳ"]="YIR";
	station["�ƾ���"]="AFP";
	station["���ׯ"]="AZK";
	station["����"]="RFH";
	station["Ӫ�ڶ�"]="YGT";
	station["����"]="YJX";
	station["����"]="YLW";
	station["������"]="YSM";
	station["���ֵ�"]="YDY";
	station["������"]="YLX";
	station["����"]="YLB";
	station["����"]="ALW";
	station["����"]="YLM";
	station["������"]="YLD";
	station["������"]="YQP";
	station["������"]="YUM";
	station["������"]="TWQ";
	station["������"]="YSY";
	station["����"]="YMF";
	station["����"]="YMN";
	station["Ԫı"]="YMM";
	station["һ��ɽ"]="YST";
	station["������"]="YXJ";
	station["����"]="YNK";
	station["����"]="YVM";
	station["������"]="YNR";
	station["һƽ��"]="YIM";
	station["Ӫ��ˮ"]="YZJ";
	station["��"]="ABM";
	station["Ӫ����"]="YPC";
	station["��Ȫ��"]="YPP";
	station["����"]="UPH";
	station["����"]="YSR";
	station["ԴǨ"]="AQK";
	station["Ҧǧ����"]="YQT";
	station["����"]="YQV";
	station["������"]="YGP";
	station["��ɽ"]="YBF";
	station["��ʯ"]="YSJ";
	station["��ʦ"]="YSF";
	station["��ˮ"]="YUK";
	station["����"]="YSV";
	station["���"]="YVH";
	station["Ҥ��"]="ASP";
	station["Ԫ��"]="YSP";
	station["������"]="YAD";
	station["Ұ����"]="AIP";
	station["������"]="YSX";
	station["����̨"]="YUT";
	station["ӥ��Ӫ��"]="YIP";
	station["Դ̶"]="YTQ";
	station["���ͱ�"]="YTZ";
	station["��Ͳɽ"]="YSL";
	station["��Ͳ��"]="YUX";
	station["��β��"]="YWM";
	station["Խ��"]="YHW";
	station["����"]="YOG";
	station["����"]="ACG";
	station["����"]="AFW";
	station["��Ҧ"]="YYH";
	station["߮����"]="YIG";
	station["������"]="YIQ";
	station["����"]="ARP";
	station["Ѽ԰"]="YYL";
	station["ԧ����"]="YYJ";
	station["�����"]="YZY";
	station["����"]="YSZ";
	station["����"]="UZH";
	station["����"]="YZK";
	station["����"]="YQM";
	station["������"]="AEM";
	station["������"]="YZD";
	station["��"]="ZEY";
	station["�ΰ�"]="ZAD";
	station["�а�"]="ZBP";
	station["�Ű���"]="ZUP";
	station["֦��"]="ZCN";
	station["�ӳ�"]="ZHY";
	station["���"]="ZQK";
	station["�޳�"]="ZIK";
	station["�Գ�"]="ZCV";
	station["�µ�"]="ZHT";
	station["�ض�"]="ZDB";
	station["�ո���"]="ZFM";
	station["�¹�̨"]="ZGD";
	station["�Թ�"]="ZGB";
	station["�к�"]="ZHX";
	station["�л���"]="VNH";
	station["֦����"]="ZIN";
	station["�ӼҴ�"]="ZJY";
	station["��ҹ�"]="ZUB";
	station["�Ͼ���"]="ZYP";
	station["�ܼ�"]="ZOB";
	station["����"]="ZDH";
	station["����"]="ZEH";
	station["�ܼ���"]="ZOD";
	station["֣����"]="ZJD";
	station["�Ҽ���"]="CWJ";
	station["տ����"]="ZWQ";
	station["���Ҥ"]="ZUJ";
	station["����ƺ��"]="ZBW";
	station["����"]="ZLT";
	station["����"]="ZIV";
	station["��³��"]="ZLD";
	station["����ŵ����"]="ZXX";
	station["��ľͷ"]="ZOQ";
	station["��Ĳ"]="ZGF";
	station["������"]="ZDJ";
	station["����"]="VNJ";
	station["������"]="ZNJ";
	station["��ƽ"]="ZPF";
	station["��ƽ"]="ZPS";
	station["����"]="ZPR";
	station["��ǿ"]="ZVP";
	station["����"]="ZQY";
	station["����"]="ZTK";
	station["���պ�"]="ZRC";
	station["������"]="ZLM";
	station["��ɽ��"]="ZGQ";
	station["������"]="ZOG";
	station["��ɽ"]="ZSQ";
	station["��ˮ"]="ZSY";
	station["��ɽ"]="ZSZ";
	station["����"]="ZSG";
	station["��̨��"]="ZZT";
	station["����"]="ZOP";
	station["��ά��"]="ZWB";
	station["����"]="ZWD";
	station["��Ϫ"]="ZOY";
	station["����"]="ZTN";
	station["��Ϫ"]="ZXS";
	station["����"]="ZVT";
	station["����"]="ZIP";
	station["�������"]="ZXC";
	station["����"]="ZVY";
	station["����"]="ZYN";
	station["��԰��"]="ZAW";
	station["��Ҵ"]="ZYJ";
	station["��Զ"]="ZUW";
	station["����Ϫ"]="ZXW";
	station["���ݶ�"]="GOS";
	station["����"]="ZUS";
	station["׳־"]="ZUX";
	station["����"]="ZZY";
	station["��կ"]="ZZM";
	station["����"]="ZXP";
	station["զ��"]="ZAL";
	station["׿��ɽ"]="ZZC";
	station["������"]="ZAQ";
	station["������"]="ADF";
	station["������"]="BMP";
	station["��������"]="DRB";
	station["����"]="DIM";
	station["���๵"]="DSD";
	station["���ݶ�"]="DOP";
	station["����"]="FDZ";
	station["��Զ"]="FYB";
	station["�߱��궫"]="GMP";
	station["���"]="GEM";
	station["������"]="IMQ";
	station["������"]="GNP";
	station["�ױڶ�"]="HFF";
	station["���й�"]="HKB";
	station["������"]="HPP";
	station["�Ϸʱ���"]="COH";
	station["���"]="HPB";
	station["����"]="IUQ";
	station["������"]="HLR";
	station["���϶�"]="HOH";
	station["����"]="KLD";
	station["�����"]="LBN";
	station["���۶�"]="MDN";
	station["ǰ��"]="QFB";
	station["��ʢ"]="QSQ";
	station["���ڱ�"]="IOQ";
	station["�����"]="XVF";
	station["Т�б�"]="XJN";
	station["��̨��"]="EDP";
	station["���綫"]="EGF";
	station["������"]="XQF";
	station["������"]="OYN";
	station["��������"]="ZHP";
	station["פ�����"]="ZLN";
	station["׿�ʶ�"]="ZDC";
	station["���ݶ�"]="ZAP";
	station["֣�ݶ�"]="ZAF";
}


void CHuoche::LoadPassanger(void)
{
    CString fullname,idcard,phone;
	CString fullname2,idcard2,phone2;
	CString fullname3,idcard3,phone3;
	this->dlg->GetDlgItem(IDC_FULLNAME)->GetWindowText(fullname);
	this->dlg->GetDlgItem(IDC_IDCARD)->GetWindowText(idcard);
	this->dlg->GetDlgItem(IDC_PHONE)->GetWindowText(phone);

	this->dlg->GetDlgItem(IDC_FULLNAME2)->GetWindowText(fullname2);
	this->dlg->GetDlgItem(IDC_IDCARD2)->GetWindowText(idcard2);
	this->dlg->GetDlgItem(IDC_PHONE2)->GetWindowText(phone2);

	this->dlg->GetDlgItem(IDC_FULLNAME3)->GetWindowText(fullname3);
	this->dlg->GetDlgItem(IDC_IDCARD3)->GetWindowText(idcard3);
	this->dlg->GetDlgItem(IDC_PHONE3)->GetWindowText(phone3);

    this->passanger_name=fullname.GetBuffer();
    this->passanger_idcard=idcard.GetBuffer();
    this->passanger_phone=phone.GetBuffer();

    this->passanger_name1=fullname2.GetBuffer();
    this->passanger_idcard1=idcard2.GetBuffer();
	this->passanger_phone1=phone2.GetBuffer();

	this->passanger_name2=fullname3.GetBuffer();
    this->passanger_idcard2=idcard3.GetBuffer();
	this->passanger_phone2=phone3.GetBuffer();

    passanger_name=echttp::UrlEncode(echttp::Utf8Encode(passanger_name));
	passanger_name1=echttp::UrlEncode(echttp::Utf8Encode(passanger_name1));
	passanger_name2=echttp::UrlEncode(echttp::Utf8Encode(passanger_name2));

}


void CHuoche::LoadDomain(void)
{
    CString domain;
	this->dlg->GetDlgItem(IDC_EDIT_DOMAIN)->GetWindowText(domain);
    this->domain=domain.GetBuffer();
    if(this->domain=="") this->domain="kyfw.12306.cn";
}


void CHuoche::CheckUserOnline(void)
{
    this->http->Post("https://"+this->domain+"/otn/login/checkUser","_json_att=",boost::bind(&CHuoche::RecvCheckUserOnline,this,_1));
}

void CHuoche::RecvCheckUserOnline(boost::shared_ptr<echttp::respone> respone)
{
    std::string result;
	if (respone->as_string()!=""){
		result=respone->as_string();

        if(result.find("data\":{\"flag\":false")!=std::string::npos)
        {
            this->showMsg("�ѵ��ߣ������µ�¼������");
        }else{
            this->showMsg("�û�����״̬����");
        }

    }else{
        this->showMsg("����û�����״̬���ؿհ�!!!");
    }
}

void CHuoche::RecvNothing(boost::shared_ptr<echttp::respone> respone)
{
    return;
}


bool CHuoche::CheckQueueCount(Ticket ticket, std::string token)
{
    std::string pstr="train_date="+echttp::UrlEncode(echttp::Date2UTC(ticket.start_train_date))+"&train_no="+ticket.train_no
                +"&stationTrainCode="+ticket.station_train_code+"&seatType="+ticket.seat_type+"&fromStationTelecode="
                +ticket.from_station_telecode+"&toStationTelecode="+ticket.to_station_telecode+"&leftTicket="+ticket.yp_info
                +"&purpose_codes=00&_json_att=&REPEAT_SUBMIT_TOKEN=5fed7925e6b4cce795f2091eaa041a90"+token;

	std::string url="https://kyfw.12306.cn/otn/confirmPassenger/getQueueCount";
	std::string queue_result=this->http->Post(url,pstr)->as_string();

    if(queue_result.find("op_2\":\"false")!=std::string::npos)
    {
        return true;
    }else{
        return false;
    }

}
