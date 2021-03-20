/*

USBRelay is a C++ header-only library for managing USB electrical relay modules
USBRelay is built over the HIDAPI interface.
 
Copyright (c) 2021 Andrey Borovsky

This library is distributed under the MIT License:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 */

#ifndef _USBRELAY_H
#define _USBRELAY_H


#include <string>
#include <vector>


namespace USBRelay {

#include <hidapi/hidapi.h>

    class USBRelayBoard {
    public:
        static std::vector<USBRelayBoard> enumerateKnownBoards(unsigned short vid = 0, unsigned short pid = 0)
        {
            struct hid_device_info *devs;
            devs = hid_enumerate(vid, pid);
            std::vector<USBRelayBoard> boards;
            auto idev = devs;
            while (idev != nullptr) {
                if (relayType(idev) != RelayType::None) {
                    boards.push_back(USBRelayBoard(idev));
                }
                idev = idev->next;
            }
            hid_free_enumeration(devs);
            return boards;
        }

        [[nodiscard]] const std::string &path() const { return _path; }

        [[nodiscard]] const std::wstring &serialNumber() const { return _serialNumber; }

        [[nodiscard]] unsigned short releaseNumber() const { return _releaseNumber; }

        [[nodiscard]] const std::wstring &manufacturerString() const { return _manufacturerString; }

        [[nodiscard]] unsigned short vendorId() const { return _vendorId; }

        [[nodiscard]] unsigned short productId() const { return _productId; }

        [[nodiscard]] int relayCount() const { return numRelays; }

        bool relayOff(int num)
        {
            if (num >= relayCount())
                return true;
            uint8_t states = getStates();
            return states & (1<<num) == 0;
        }

        bool relayOn(int num) {return !relayOff(num);}

        void setRelayState(int num, bool on)
        {
            if (num < numRelays)
                setState(num, on ? CmdOn : CmdOff);
        }

        void setRelayOff(int num)
        {
            if (num < numRelays)
                setState(num, CmdOff);
        }

        void setRelayOn(int num)
        {
            if (num < numRelays)
                setState(num, CmdOn);
        }

        void setAllRelaysOff()
        {
            setState(0, CmdAllOff);
        }

        void setAllRelaysOn()
        {
            setState(0, CmdAllOn);
        }


        void toggleRelay(int n)
        {
            if (relayOn(n))
                setRelayOff(n);
            else
                setRelayOn(n);
        }
        const std::string  &userSerial()
        {
            getStates();
            return _userSerial;
        }
        void setUserSerial(const std::string & sn)
        {
            DeviceHandle dh(_path.c_str());
            uint8_t buf[9]{0, 0xfa, 0,0,0,0,0,0,0};
            for(int i = 0; i  < SerialLength && i < sn.length(); ++i)
                buf[2+i] = sn[i];
            hid_write(dh.handle(), buf, sizeof (buf));
        }
    private:
        enum class RelayType {
            None = 0,
            DCT,
            UCR
        };
        class DeviceHandle
        {
        public:
            DeviceHandle(const char * path) {
                _h = hid_open_path(path);
            }
            ~DeviceHandle() {hid_close(_h);}
            hid_device * handle() {return _h;}
            hid_device * _h;
        };
        RelayType _type;
        std::string _path;
        int numRelays;
        std::wstring _serialNumber;
        unsigned short _releaseNumber;
        std::wstring _manufacturerString;
        unsigned short _vendorId;
        unsigned short _productId;
        std::string _userSerial;
        static const int SerialLength = 5;
        static const int StateOffset = 7;
        static const uint8_t CmdGetFeatures = 1;
        static const uint8_t CmdAllOn = 0xfe;
        static const uint8_t CmdAllOff = 0xfc;
        static const uint8_t CmdOn = 0xff;
        static const uint8_t CmdOff = 0xfd;
        static const uint8_t CmdSetSerial = 0xfa;
    private:
        USBRelayBoard(struct hid_device_info *dinfo) :
                _type(relayType(dinfo)),
                _path(dinfo->path),
                _serialNumber(dinfo->serial_number),
                _releaseNumber(dinfo->release_number),
                _manufacturerString(dinfo->manufacturer_string),
                _vendorId(dinfo->vendor_id),
                _productId(dinfo->product_id) {
            if (_type == RelayType::DCT)
                numRelays = std::stoi(std::wstring(dinfo->product_string).substr(8));
            else
                numRelays = _type == RelayType::UCR ? 9 : 0;
        }

        static RelayType relayType(struct hid_device_info *dinfo) {
            if (std::wstring(dinfo->product_string).find(L"USBRelay") == 0)
                return RelayType::DCT;
            if (std::wstring(dinfo->product_string).find(L"HIDRelay") == 0)
                return RelayType::UCR;
            return RelayType::None;
        }
        uint8_t getStates()
        {
            DeviceHandle dh(_path.c_str());
            uint8_t buf[9]{};
            //auto _h = hid_open_path(_path.c_str());
            buf[0] = CmdGetFeatures;
            if(int ret = hid_get_feature_report(dh.handle(), buf, sizeof(buf)); ret == -1) {
                perror("hid_get_feature_report\n");
            } else {
                if (_type == RelayType::UCR)
                    return ret;
            }
            _userSerial = std::string((char*)&buf[0]);
            return buf[StateOffset];
        }
        void setState(uint8_t num, uint8_t state)
        {
            DeviceHandle dh(_path.c_str());
            if (_type == RelayType::DCT) {
                uint8_t buf[9]{0, state, (uint8_t)(num+1), 0, 0, 0, 0, 0, 0};
                if (auto res = hid_write(dh.handle(), buf, sizeof (buf)); res < 0) {
                    printf(".%i.", res);
                }
            } else {
                uint8_t b = (state != 0 ? (uint8_t)0xf0 : (uint8_t)0) + (uint8_t)num;
                uint8_t buf[9]{0, b, 0, 0, 0, 0, 0, 0, 0};
                if (auto res = hid_write(dh.handle(), buf, sizeof (buf)); res < 0) {
                }
            }
        }
    };



    class UsbRelay {

    };

}
#endif //_USBRELAY_H
