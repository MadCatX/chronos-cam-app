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
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <QDebug>
#include <semaphore.h>
#include <QSettings>
#include <QResource>
#include <QDir>
#include <QScreen>
#include <QIODevice>
#include <QApplication>

#include "font.h"
#include "camera.h"
#include "util.h"
#include "types.h"
#include "defines.h"
#include <QWSDisplay>
#include "control.h"
#include "camera.h"
#include "cammainwindow.h"

Camera::Camera()
{
	QSettings appSettings;

	playbackMode = false;
	recording = false;
	recordingData.ignoreSegments = 0;
	recordingData.hasBeenSaved = true;
	recordingData.hasBeenViewed = true;
	unsavedWarnEnabled = getUnsavedWarnEnable();
	autoSave = appSettings.value("camera/autoSave", 0).toBool();
	autoRecord = appSettings.value("camera/autoRecord", 0).toBool();
	ButtonsOnLeft = getButtonsOnLeft();
	UpsideDownDisplay = getUpsideDownDisplay();
	strcpy(serialNumber, "Not_Set");
}

Camera::~Camera()
{
}

CameraErrortype Camera::init(Video * vinstInst, Control * cinstInst)
{
	CameraErrortype retVal;
	QSettings appSettings;
	CameraStatus cs;

	vinst = vinstInst;
	cinst = cinstInst;
	//Read serial number in
	retVal = (CameraErrortype)readSerialNumber(serialNumber);
	if(retVal != SUCCESS)
		return retVal;

	/* Load the sensor information */
	cinst->getSensorInfo(&sensorInfo);

	/* Color detection or override from env. */
	const char *envColor = getenv("CAM_OVERRIDE_COLOR");
	if (envColor) {
		char *endp;
		unsigned long uval = strtoul(envColor, &endp, 0);
		if (*endp == '\0') isColor = (uval != 0);
		else if (strcasecmp(envColor, "COLOR") == 0) isColor = true;
		else if (strcasecmp(envColor, "TRUE") == 0) isColor = true;
		else if (strcasecmp(envColor, "MONO") == 0) isColor = false;
		else if (strcasecmp(envColor, "FALSE") == 0) isColor = false;
		else isColor = sensorInfo.cfaPattern != "mono";
	}
	else {
		isColor = sensorInfo.cfaPattern != "mono";
	}

	retVal = cinst->status(&cs);
	recording = !strcmp(cs.state, "idle");

	vinst->bitsPerPixel        = appSettings.value("recorder/bitsPerPixel", 0.7).toDouble();
	vinst->maxBitrate          = appSettings.value("recorder/maxBitrate", 40.0).toDouble();
	vinst->framerate           = appSettings.value("recorder/framerate", 60).toUInt();
	strcpy(cinst->filename,      appSettings.value("recorder/filename", "").toString().toAscii());
	strcpy(cinst->fileDirectory, appSettings.value("recorder/fileDirectory", "").toString().toAscii());
	strcpy(cinst->fileFolder,    appSettings.value("recorder/fileFolder", "").toString().toAscii());
	if(strlen(cinst->fileDirectory) == 0){
		/* Set the default file path, or fall back to the MMC card. */
		int i;
		bool fileDirFoundOnUSB = false;
		for (i = 1; i <= 3; i++) {
			sprintf(cinst->fileDirectory, "/media/sda%d", i);
			if (path_is_mounted(cinst->fileDirectory)) {
				fileDirFoundOnUSB = true;
				break;
			}
		}
		if(!fileDirFoundOnUSB) strcpy(cinst->fileDirectory, "/media/mmcblk1p1");
	}

	/* Connect to any parameter updates that we care to receive. */
	cinst->listen("state", this, SLOT(api_state_valueChanged(const QVariant &)));
	cinst->listen("wbMatrix", this, SLOT(api_wbMatrix_valueChanged(const QVariant &)));
	cinst->listen("colorMatrix", this, SLOT(api_colorMatrix_valueChanged(const QVariant &)));

	//vinst->setDisplayOptions(getZebraEnable(), getFocusPeakEnable());
	vinst->setDisplayPosition(ButtonsOnLeft ^ UpsideDownDisplay);
	usleep(2000000); //needed temporarily with current pychronos
	cinst->doReset(); //also needed temporarily

	printf("Video init done\n");
	int fpColor = getFocusPeakColor();
	if (fpColor == 0)
	{
		setFocusPeakColor(2);	//set to cyan, if starts out set to black
	}
	return SUCCESS;
}

/* Frame size validation function. */
bool Camera::isValidResolution(FrameGeometry *size)
{
	/* Enforce resolution limits. */
	if ((size->hRes < sensorInfo.hMinimum) || (size->hRes + size->hOffset > sensorInfo.geometry.hRes)) {
		return false;
	}
	if ((size->vRes < sensorInfo.vMinimum) || (size->vRes + size->vOffset > sensorInfo.geometry.vRes)) {
		return false;
	}
	if (size->vDarkRows > sensorInfo.geometry.vDarkRows) {
		return false;
	}
	if (size->bitDepth != sensorInfo.geometry.bitDepth) {
		return false;
	}

	/* Enforce minimum pixel increments. */
	if ((size->hRes % sensorInfo.hIncrement) || (size->hOffset % sensorInfo.hIncrement)) {
		return false;
	}
	if ((size->vRes % sensorInfo.vIncrement) || (size->vOffset % sensorInfo.vIncrement)) {
		return false;
	}
	if (size->vDarkRows % sensorInfo.vIncrement) {
		return false;
	}

	/* Otherwise, the resultion and offset are valid. */
	return true;
}

UInt32 Camera::setImagerSettings(ImagerSettings_t settings)
{
	QString modes[] = {"normal", "segmented", "burst"};
	QVariantMap values;
	QVariantMap resolution;

	if(!isValidResolution(&settings.geometry) ||
		settings.recRegionSizeFrames < RECORD_LENGTH_MIN ||
		settings.segments > settings.recRegionSizeFrames) {
		return CAMERA_INVALID_IMAGER_SETTINGS;
	}

	resolution.insert("hRes", QVariant(settings.geometry.hRes));
	resolution.insert("vRes", QVariant(settings.geometry.vRes));
	resolution.insert("hOffset", QVariant(settings.geometry.hOffset));
	resolution.insert("vOffset", QVariant(settings.geometry.vOffset));
	resolution.insert("vDarkRows", QVariant(settings.geometry.vDarkRows));
	resolution.insert("bitDepth", QVariant(settings.geometry.bitDepth));
	resolution.insert("minFrameTime", QVariant(settings.geometry.minFrameTime));

	values.insert("resolution", QVariant(resolution));
	values.insert("framePeriod", QVariant(settings.period));
	values.insert("currentGain", QVariant(1 << settings.gain));
	values.insert("exposurePeriod", QVariant(settings.exposure));
	if (settings.mode > 3 ) qFatal("imagerSetting mode is FPN");
	else values.insert("recMode", modes[settings.mode]);
	values.insert("recSegments", QVariant(settings.segments));
	values.insert("recMaxFrames", QVariant(settings.recRegionSizeFrames));
	values.insert("recTrigDelay", QVariant(settings.recTrigDelay));

	CameraErrortype retVal = cinst->setPropertyGroup(values);

	return retVal;
}

UInt32 Camera::getMaxRecordRegionSizeFrames(FrameGeometry *geometry)
{
	FrameTiming timing;

	if (cinst->getTiming(geometry, &timing) != SUCCESS) {
		qDebug("Failed to get recording region size.");
		return 0;
	}
	return timing.cameraMaxFrames;
}

UInt32 Camera::setIntegrationTime(double intTime, FrameGeometry *fSize, Int32 flags)
{
	cinst->setInt("exposurePeriod", intTime * 1e9);
	return SUCCESS;
}

void Camera::updateVideoPosition()
{
	vinst->setDisplayPosition(ButtonsOnLeft ^ UpsideDownDisplay);
}

Int32 Camera::startRecording(void)
{
	qDebug("===== Camera::startRecording()");

	QString str;

	FrameGeometry geometry;
	geometry.hRes          = 1280;
	geometry.vRes          = 1024;
	geometry.hOffset       = 0;
	geometry.vOffset       = 0;
	geometry.vDarkRows     = 0;
	geometry.bitDepth	   = 12;
	geometry.minFrameTime  = 0.0002; //arbitrary here!

	cinst->stopRecording();

	if(recording)
		return CAMERA_ALREADY_RECORDING;
	if(playbackMode)
		return CAMERA_IN_PLAYBACK_MODE;

	recordingData.valid = false;
	recordingData.hasBeenSaved = false;
	vinst->flushRegions();

	cinst->startRecording();

	recording = true;
	recordingData.hasBeenViewed = false;
	recordingData.ignoreSegments = 0;

	return SUCCESS;
}

Int32 Camera::stopRecording(void)
{
	if(!recording)
		return CAMERA_NOT_RECORDING;

	cinst->stopRecording();
	//recording = false;

	return SUCCESS;
}

UInt32 Camera::setPlayMode(bool playMode)
{
	if(recording)
		return CAMERA_ALREADY_RECORDING;
	if(!recordingData.valid)
		return CAMERA_NO_RECORDING_PRESENT;

	playbackMode = playMode;

	if(playMode)
	{
		vinst->setPosition(0);
	}
	else
	{
		//bool videoFlip = (sensor->getSensorQuirks() & SENSOR_QUIRK_UPSIDE_DOWN) != 0;
		bool videoFlip = false;
		vinst->liveDisplay(videoFlip);
	}
	return SUCCESS;
}

void Camera::setBncDriveLevel(UInt32 level)
{
	QVariantMap config;

	config.insert("driveStrength", QVariant(level ? 3 : 0));
	config.insert("debounce", QVariant(true));
	config.insert("invert", QVariant(false));
	config.insert("source", QVariant("alwaysHigh"));

	cinst->setProperty("ioMappingIo1", config);
}

#define COLOR_MATRIX_MAXVAL	((1 << SENSOR_DATA_WIDTH) * (1 << COLOR_MATRIX_INT_BITS))

void Camera::setCCMatrix(const double *matrix)
{
	double ccmd[9];

	int ccm[] = {
		/* First row */
		(int)(4096.0 * matrix[0]),
		(int)(4096.0 * matrix[1]),
		(int)(4096.0 * matrix[2]),
		/* Second row */
		(int)(4096.0 * matrix[3]),
		(int)(4096.0 * matrix[4]),
		(int)(4096.0 * matrix[5]),
		/* Third row */
		(int)(4096.0 * matrix[6]),
		(int)(4096.0 * matrix[7]),
		(int)(4096.0 * matrix[8])
	};

	/* Check if the colour matrix has clipped, and scale it back down if necessary. */
	int i;
	int peak = 4096;
	for (i = 0; i < 9; i++) {
		if (ccm[i] > peak) peak = ccm[i];
		if (-ccm[i] > peak) peak = -ccm[i];
	}
	if (peak > COLOR_MATRIX_MAXVAL) {
		fprintf(stderr, "Warning: Color matrix has clipped - scaling down to fit\n");
		for (i = 0; i < 9; i++) ccm[i] = (ccm[i] * COLOR_MATRIX_MAXVAL) / peak;
	}

	fprintf(stderr, "Setting Color Matrix:\n");
	fprintf(stderr, "\t%06f %06f %06f\n",   ccm[0] / 4096.0, ccm[1] / 4096.0, ccm[2] / 4096.0);
	fprintf(stderr, "\t%06f %06f %06f\n",   ccm[3] / 4096.0, ccm[4] / 4096.0, ccm[5] / 4096.0);
	fprintf(stderr, "\t%06f %06f %06f\n\n", ccm[6] / 4096.0, ccm[7] / 4096.0, ccm[8] / 4096.0);

	for (i = 0; i < 9; i++) ccmd[i] = ccm[i] / 4096.0;
	cinst->setArray("colorMatrix", 9, (double *)&ccmd);
}

void Camera::setWhiteBalance(const double *rgb)
{
	qDebug("Setting WB Matrix: %06f %06f %06f", rgb[0], rgb[1], rgb[2]);
	cinst->setArray("wbMatrix", 3, rgb);
}

Int32 Camera::autoWhiteBalance(UInt32 x, UInt32 y)
{
	cinst->startAutoWhiteBalance();
}

UInt8 Camera::getWBIndex(){
	QSettings appsettings;
	return appsettings.value("camera/WBIndex", 2).toUInt();
}

void Camera::setWBIndex(UInt8 index){
	QSettings appsettings;
	appsettings.setValue("camera/WBIndex", index);
}


void Camera::setFocusAid(bool enable)
{
	/* FIXME: Not implemented */
}

bool Camera::getFocusAid()
{
	/* FIXME: Not implemented */
	return false;
}

/* Nearest multiple rounding */
static inline UInt32
round(UInt32  x, UInt32 mult)
{
	UInt32 offset = (x) % (mult);
	return (offset >= mult/2) ? x - offset + mult : x - offset;
}

Int32 Camera::blackCalAllStdRes(bool factory, QProgressDialog *dialog)
{
	int g;
	ImagerSettings_t settings;
	FrameGeometry maxSize = sensorInfo.geometry;
	FrameTiming timing;

	//Populate the common resolution combo box from the list of resolutions
	QFile fp;
	UInt32 retVal = SUCCESS;
	QStringList resolutions;
	QString filename;
	QString line;

	filename.append("camApp:resolutions");
	QFileInfo resolutionsFile(filename);
	if (resolutionsFile.exists() && resolutionsFile.isFile()) {
		fp.setFileName(filename);
		fp.open(QIODevice::ReadOnly);
		if(!fp.isOpen()) {
			qDebug("Error: resolutions file couldn't be opened");
			return CAMERA_FILE_ERROR;
		}
	}
	else {
		qDebug("Error: resolutions file isn't present");
		return CAMERA_FILE_ERROR;
	}

	/* Read the resolutions file into a list. */
	while (true) {
		QString tmp = fp.readLine(30);
		QStringList strlist;
		FrameGeometry size;

		/* Try to read another resolution from the file. */
		if (tmp.isEmpty() || tmp.isNull()) break;
		strlist = tmp.split('x');
		if (strlist.count() < 2) break;

		/* If it's a supported resolution, add it to the list. */
		size.hRes = strlist[0].toInt(); //pixels
		size.vRes = strlist[1].toInt(); //pixels
		size.vDarkRows = 0;	//dark rows
		size.bitDepth = maxSize.bitDepth;
		size.hOffset = round((maxSize.hRes - size.hRes) / 2, sensorInfo.hIncrement);
		size.vOffset = round((maxSize.vRes - size.vRes) / 2, sensorInfo.vIncrement);
		if (isValidResolution(&size)) {
			line.sprintf("%ux%u", size.hRes, size.vRes);
			resolutions.append(line);
		}
	}
	fp.close();

	/* Ensure that the maximum sensor size is also included. */
	line.sprintf("%ux%u", maxSize.hRes, maxSize.vRes);
	if (!resolutions.contains(line)) {
		resolutions.prepend(line);
	}

	/* If we have a progress dialog - figure out how many calibration steps to do. */
	if (dialog) {
		int maxcals = 0;

		/* Count up the number of gain settings to calibrate for. */
		for (g = sensorInfo.minGain; g <= sensorInfo.maxGain; g *= 2) {
			 maxcals += resolutions.count();
		}

		dialog->setMaximum(maxcals);
		dialog->setValue(0);
		dialog->setAutoClose(false);
		dialog->setAutoReset(false);
	}

	/* Disable the video port during calibration. */
	vinst->pauseDisplay();

	//For each gain
	int progress = 0;
	for(g = sensorInfo.minGain; g <= sensorInfo.maxGain; g *= 2)
	{
		QStringListIterator iter(resolutions);
		while (iter.hasNext()) {
			QString value = iter.next();
			QStringList tmp = value.split('x');

			// Split the resolution string on 'x' into horizontal and vertical sizes.
			settings.geometry.hRes = tmp[0].toInt(); //pixels
			settings.geometry.vRes = tmp[1].toInt(); //pixels
			settings.geometry.vDarkRows = 0;	//dark rows
			settings.geometry.bitDepth = maxSize.bitDepth;
			settings.geometry.hOffset = round((maxSize.hRes - settings.geometry.hRes) / 2, sensorInfo.hIncrement);
			settings.geometry.vOffset = round((maxSize.vRes - settings.geometry.vRes) / 2, sensorInfo.vIncrement);

			// Get the resolution timing limits.
			cinst->getTiming(&settings.geometry, &timing);
			settings.gain = g;
			settings.recRegionSizeFrames = timing.cameraMaxFrames;
			settings.period = timing.minFramePeriod;
			settings.exposure = timing.exposureMax;
			settings.disableRingBuffer = 0;
			settings.mode = RECORD_MODE_NORMAL;
			settings.prerecordFrames = 1;
			settings.segmentLengthFrames = timing.cameraMaxFrames;
			settings.segments = 1;

			/* Update the progress dialog. */
			progress++;
			if (dialog) {
				QString label;
				if (dialog->wasCanceled()) {
					goto exit_calibration;
				}
				label.sprintf("Computing calibration for %ux%u at x%d gain",
							  settings.geometry.hRes, settings.geometry.vRes, settings.gain);
				dialog->setValue(progress);
				dialog->setLabelText(label);
				QCoreApplication::processEvents();
			}

			retVal = setImagerSettings(settings);
			if (SUCCESS != retVal)
				return retVal;

			qDebug("Doing calibration for %ux%u...", settings.geometry.hRes, settings.geometry.vRes);
			cinst->startCalibration({"analogCal", "blackCal"}, true);
			bool calWaiting;
			do {
				QString state;
				cinst->getString("state", &state);
				calWaiting = state != "idle";

				struct timespec t = {0, 50000000};
				nanosleep(&t, NULL);

			} while (calWaiting);

			qDebug() << "Done.";
		}
	}
exit_calibration:

	fp.close();

	cinst->getTiming(&maxSize, &timing);
	memcpy(&settings.geometry, &maxSize, sizeof(maxSize));
	settings.geometry.vDarkRows = 0; //Inactive dark rows on top
	settings.gain = sensorInfo.minGain;
	settings.recRegionSizeFrames = timing.cameraMaxFrames;
	settings.period = timing.minFramePeriod;
	settings.exposure = timing.exposureMax;

	retVal = setImagerSettings(settings);
	vinst->liveDisplay(false);
	if(SUCCESS != retVal) {
		qDebug("blackCalAllStdRes: Error during setImagerSettings %d", retVal);
		return retVal;
	}

	return SUCCESS;
}

bool Camera::getButtonsOnLeft(void){
	QSettings appSettings;
	return (appSettings.value("camera/ButtonsOnLeft", false).toBool());
}

void Camera::setButtonsOnLeft(bool en){
	QSettings appSettings;
	ButtonsOnLeft = en;
	appSettings.setValue("camera/ButtonsOnLeft", en);
}

bool Camera::getUpsideDownDisplay(){
	QSettings appSettings;
	return (appSettings.value("camera/UpsideDownDisplay", false).toBool());
}

void Camera::setUpsideDownDisplay(bool en){
	QSettings appSettings;
	UpsideDownDisplay = en;
	appSettings.setValue("camera/UpsideDownDisplay", en);
}

bool Camera::RotationArgumentIsSet()
{
	QScreen *qscreen = QScreen::instance();
	return qscreen->classId() == QScreen::TransformedClass;
}


void Camera::upsideDownTransform(int rotation){
	QWSDisplay::setTransformation(rotation);
}

bool Camera::getFocusPeakEnable(void)
{
	double level;
	cinst->getFloat("focusPeakingLevel", &level);
	return (level > 0.1);

}

void Camera::setFocusPeakEnable(bool en)
{
	cinst->setFloat("focusPeakingLevel", en ? focusPeakLevel : 0.0);
	focusPeakEnabled = en;
	//vinst->setDisplayOptions(zebraEnabled, focusPeakEnabled);
}

int Camera::getFocusPeakColor(){
	return getFocusPeakColorLL();
}

void Camera::setFocusPeakColor(int value){
	setFocusPeakColorLL(value);
	focusPeakColorIndex = value;
}


bool Camera::getZebraEnable(void)
{
	//QSettings appSettings;
	//return appSettings.value("camera/zebra", true).toBool();
	double zebra;
	cinst->getFloat("zebraLevel", &zebra);
	return zebra > 0.0;
}

void Camera::setZebraEnable(bool en)
{
	QSettings appSettings;
	zebraEnabled = en;
	appSettings.setValue("camera/zebra", en);
	//vinst->setZebra(zebraEnabled);
	cinst->setFloat("zebraLevel", en ? 0.05 : 0.0);
}

bool Camera::getFanDisable(void)
{
	QSettings appSettings;
	return appSettings.value("camera/fanDisabled", true).toBool();
}

void Camera::setFanDisable(bool en)
{
	QSettings appSettings;
	fanDisabled = en;
	appSettings.setValue("camera/fanDisabled", en);
	//TODO: cinst->setInt("fanSpeed", arg1);
}

int Camera::getUnsavedWarnEnable(void){
	QSettings appSettings;
	return appSettings.value("camera/unsavedWarn", 1).toInt();
	//If there is unsaved video in RAM, prompt to start record.  2=always, 1=if not reviewed, 0=never
}

void Camera::setUnsavedWarnEnable(int newSetting){
	QSettings appSettings;
	unsavedWarnEnabled = newSetting;
	appSettings.setValue("camera/unsavedWarn", newSetting);
}

int Camera::getAutoPowerMode(void){
	QSettings appSettings;
	return appSettings.value("camera/autoPowerMode", 1).toInt();
	//0 = disabled, 1 = turn on if AC inserted, 2 = turn off if AC removed, 3 = both
}

void Camera::setAutoPowerMode(int newSetting){
	qDebug() << "AutoPowerMode:" << newSetting;
	QSettings appSettings;
	autoPowerMode = newSetting;
	appSettings.setValue("camera/autoPowerMode", newSetting);
	bool mainsConnectTurnOn = newSetting & 1;
	bool mainsDisconnectTurnOff = newSetting & 2;
	cinst->setBool("powerOnWhenMainsConnected", mainsConnectTurnOn);
	cinst->setBool("powerOffWhenMainsDisconnected", mainsDisconnectTurnOff);
}

int Camera::getAutoSavePercent(void){
	QSettings appSettings;
	return appSettings.value("camera/autoAutoSavePercent", 1).toInt();
}

void Camera::setAutoSavePercent(int newSetting){
	QSettings appSettings;
	autoSavePercent = newSetting;
	appSettings.setValue("camera/autoAutoSavePercent", newSetting);
	cinst->setFloat("saveAndPowerDownLowBatteryLevelPercent", (double)newSetting);
}

bool Camera::getAutoSavePercentEnabled(void){
	QSettings appSettings;
	return appSettings.value("camera/autoAutoSavePercentEnabled", 1).toBool();
}

void Camera::setAutoSavePercentEnabled(bool newSetting){
	QSettings appSettings;
	autoSavePercentEnabled = newSetting;
	appSettings.setValue("camera/autoAutoSavePercent", newSetting);
	cinst->setBool("saveAndPowerDownWhenLowBattery", newSetting);
}

bool Camera::getShippingMode(void){
	QSettings appSettings;
	return appSettings.value("camera/shippingMode", 1).toBool();
	//0 = disabled, 1 = turn on if AC inserted, 2 = turn off if AC removed, 3 = both
}

void Camera::setShippingMode(int newSetting){
	QSettings appSettings;
	shippingMode = newSetting;
	appSettings.setValue("camera/shippingMode", newSetting);
	cinst->setBool("shippingMode", newSetting);
}



void Camera::set_autoSave(bool state) {
	QSettings appSettings;
	autoSave = state;
	appSettings.setValue("camera/autoSave", state);
}

bool Camera::get_autoSave() {
	QSettings appSettings;
	return appSettings.value("camera/autoSave", false).toBool();
}


void Camera::set_autoRecord(bool state) {
	QSettings appSettings;
	autoRecord = state;
	appSettings.setValue("camera/autoRecord", state);
}

bool Camera::get_autoRecord() {
	QSettings appSettings;
	return appSettings.value("camera/autoRecord", false).toBool();
}


void Camera::set_demoMode(bool state) {
	QSettings appSettings;
	demoMode = state;
	appSettings.setValue("camera/demoMode", state);
}

bool Camera::get_demoMode() {
	QSettings appSettings;
	return appSettings.value("camera/demoMode", false).toBool();
}

void Camera::api_state_valueChanged(const QVariant &value)
{
	QString state = value.toString();

	qDebug() << "state change:" << lastState << "=>" << state;

	recording = (state == "recording");
	lastState = state;
}

void Camera::api_wbMatrix_valueChanged(const QVariant &wb)
{
	QDBusArgument dbusArgs = wb.value<QDBusArgument>();
	dbusArgs.beginArray();
	for (int i=0; i<3; i++)
	{
		whiteBalMatrix[i] = dbusArgs.asVariant().toDouble();
	}
	dbusArgs.endArray();
}

void Camera::api_colorMatrix_valueChanged(const QVariant &wb)
{
	QDBusArgument dbusArgs = wb.value<QDBusArgument>();
	dbusArgs.beginArray();
	for (int i=0; i<9; i++)
	{
		colorCalMatrix[i] = dbusArgs.asVariant().toDouble();
	}
	dbusArgs.endArray();
}

void Camera::on_chkLiveLoop_stateChanged(int arg1)
{
	if (arg1)
	{
		//enable loop timer
		loopTimer = new QTimer(this);
		connect(loopTimer, SIGNAL(timeout()), this, SLOT(onLoopTimer()));
		loopTimer->start(2000);
		loopTimerEnabled = true;

	}
	else
	{
		//bool videoFlip = (sensor->getSensorQuirks() & SENSOR_QUIRK_UPSIDE_DOWN) != 0;
		bool videoFlip = false;
		//disable loop timer
		loopTimer->stop();
		delete loopTimer;
		vinst->liveDisplay(videoFlip);
		loopTimerEnabled = false;

	}

}


void Camera::onLoopTimer()
{
	//record snippet
	qDebug() << "loop timer";

	cinst->startRecording();

	struct timespec t = {0, 150000000};
	nanosleep(&t, NULL);

	cinst->stopRecording();

	//play back snippet
	vinst->loopPlayback(1, 400, 30);
}

void Camera::on_spinLiveLoopTime_valueChanged(int arg1)
{
	if (arg1 < 250) //minimum time
	{
		return;
	}

	if (loopTimerEnabled)
	{
		//disable loop timer
		loopTimer->stop();
		delete loopTimer;

		//re-enable loop timer
		loopTimer = new QTimer(this);
		connect(loopTimer, SIGNAL(timeout()), this, SLOT(onLoopTimer()));
		loopTimer->start(arg1);
	}
}
