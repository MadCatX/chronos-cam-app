/****************************************************************************
 *  Copyright (C) 2013-2017 Kron Technologies Inc <http://www.krontech.ca>. *
 *                                                                          *
 *  This program is free software: you can redistribute it and/or modify    *
 *  it under the terms of the GNU General Public License as published by    *
 *  the Free Software Foundation, either version 3 of the License, or       *
 *  (at your option) any later version.                                     *
 *                                                                          *
 *  This program is distributed in the hope that it will be useful,         *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU General Public License for more details.                            *
 *                                                                          *
 *  You should have received a copy of the GNU General Public License       *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 ****************************************************************************/
#include "camera.h"
#include "cameraRegisters.h"

#include "ramwindow.h"
#include "ui_ramwindow.h"

#include "util.h"

ramWindow::ramWindow(QWidget *parent, Camera * cameraInst) :
	QWidget(parent),
	ui(new Ui::ramWindow)
{
	ui->setupUi(this);
	camera = cameraInst;

	connect(ui->cmdClose, SIGNAL(clicked()), this, SLOT(close()));

	UInt32 rpt = 0;//camera->gpmc->read32(READ_PULSE_TAP_ADDR);

	ui->spinRPT0->setValue((rpt >> 3*0) & 0x7);
	ui->spinRPT1->setValue((rpt >> 3*1) & 0x7);
	ui->spinRPT2->setValue((rpt >> 3*2) & 0x7);
	ui->spinRPT3->setValue((rpt >> 3*3) & 0x7);
	ui->spinRPT4->setValue((rpt >> 3*4) & 0x7);
	ui->spinRPT5->setValue((rpt >> 3*5) & 0x7);
	ui->spinRPT6->setValue((rpt >> 3*6) & 0x7);
	ui->spinRPT7->setValue((rpt >> 3*7) & 0x7);

	//disable video readout
	camera->gpmc->write32(DISPLAY_CTL_ADDR, camera->gpmc->read32(DISPLAY_CTL_ADDR) | DISPLAY_CTL_READOUT_INH_MASK);
	camera->gpmc->setTimeoutEnable(true);

}

ramWindow::~ramWindow()
{
	//enable video readout
	camera->gpmc->write32(DISPLAY_CTL_ADDR, camera->gpmc->read32(DISPLAY_CTL_ADDR) & ~DISPLAY_CTL_READOUT_INH_MASK);
	camera->gpmc->setTimeoutEnable(false);
	delete ui;
}

void ramWindow::on_cmdSet_clicked()
{
	UInt32 rpt;

	rpt =	(ui->spinRPT0->value() << 3*0) |
			(ui->spinRPT1->value() << 3*1) |
			(ui->spinRPT2->value() << 3*2) |
			(ui->spinRPT3->value() << 3*3) |
			(ui->spinRPT4->value() << 3*4) |
			(ui->spinRPT5->value() << 3*5) |
			(ui->spinRPT6->value() << 3*6) |
			(ui->spinRPT7->value() << 3*7);

	//camera->gpmc->write32(READ_PULSE_TAP_ADDR, rpt);

}

void ramWindow::on_cmdRead_clicked()
{
	camera->gpmc->write32(GPMC_PAGE_OFFSET_ADDR, 0);

	//camera->gpmc->readRam16(0x20);
	camera->gpmc->readRam16(0x10020);

}
