#pragma once
#include <string>


// CYzDlg �Ի���
class CHuoche;

class CYzDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CYzDlg)

public:
	CYzDlg(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CYzDlg();

// �Ի�������
	enum { IDD = IDD_YZDLG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CHuoche* huoche;
	std::string yzcode;
    std::string file_path;
	afx_msg void OnBnClickedButton1();
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnStnClickedPic2();
	void loadImg(void);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};
