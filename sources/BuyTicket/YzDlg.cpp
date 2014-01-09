// YzDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "BuyTicket.h"
#include "YzDlg.h"
#include "afxdialogex.h"
#include "Huoche.h"


// CYzDlg �Ի���

IMPLEMENT_DYNAMIC(CYzDlg, CDialogEx)

CYzDlg::CYzDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CYzDlg::IDD, pParent)
{

}

CYzDlg::~CYzDlg()
{
}

void CYzDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CYzDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON1, &CYzDlg::OnBnClickedButton1)
	ON_WM_PAINT()
	ON_STN_CLICKED(IDC_PIC2, &CYzDlg::OnStnClickedPic2)
END_MESSAGE_MAP()


// CYzDlg ��Ϣ�������


void CYzDlg::OnBnClickedButton1()
{
	CString code;
	this->GetDlgItem(IDC_EDIT_IMG2)->GetWindowText(code);
	this->yzcode=code.GetBuffer();

	if(this->yzcode=="")
	{
		this->MessageBox("���벻��Ϊ��!");
	}
	else
	{
		this->OnOK();
	}
}


BOOL CYzDlg::OnInitDialog()
{

	return CDialogEx::OnInitDialog();
}


void CYzDlg::OnPaint()
{
	loadImg();
	
	CDialogEx::OnPaint();
}


void CYzDlg::OnStnClickedPic2()
{
    this->file_path=huoche->loadCode2();
	this->loadImg();
}


void CYzDlg::loadImg(void)
{
	CRect rect;
	this->GetDlgItem(IDC_PIC2)->GetClientRect(&rect);     //m_pictureΪPicture Control�ؼ���������ÿؼ����������

	CImage image;       //ʹ��ͼƬ��
	image.Load(this->file_path.c_str());   //װ��·����ͼƬ��Ϣ��ͼƬ��
    if(!image.IsNull()){

        CDC* pDC = this->GetDlgItem(IDC_PIC2)->GetWindowDC();    //�����ʾ�ؼ���DC
	    image.Draw( pDC -> m_hDC,rect);      //ͼƬ���ͼƬ����Draw����
	
	    ReleaseDC(pDC);
    }
	
}


BOOL CYzDlg::PreTranslateMessage(MSG* pMsg)
{
	if(pMsg -> message == WM_KEYDOWN)
	{
		CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_IMG2);
        ASSERT(pEdit);
        if(pMsg->hwnd == pEdit->GetSafeHwnd() && VK_RETURN == pMsg->wParam)
        {
            this->OnBnClickedButton1();
            return TRUE;
        }
	}	

	return CDialogEx::PreTranslateMessage(pMsg);
}
