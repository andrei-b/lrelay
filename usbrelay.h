//
// Created by andrey on 3/14/21.
//

#ifndef _USBRELAY_H
#define _USBRELAY_H


#include <string>
#include <vector>


namespace USBRelay {

#include <hidapi/hidapi.h>




    class USBRelayBoard {
    public:
        static std::vector<USBRelayBoard> enumerateKnownBoards(unsigned short vid = 0, unsigned short pid = 0) {
            struct hid_device_info *devs;
            devs = hid_enumerate(vid, pid);
            std::vector<USBRelayBoard> boards;
            auto idev = devs;
            while (idev != nullptr) {
                if (relayType(idev) != RelayType::None) {
                    boards.emplace_back(idev);
                }
                idev = idev->next;
            }
            hid_free_enumeration(devs);
            return boards;
        }
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

        [[nodiscard]] const std::string &path() const { return _path; }

        [[nodiscard]] const std::wstring &serialNumber() const { return _serialNumber; }

        [[nodiscard]] unsigned short releaseNumber() const { return _releaseNumber; }

        [[nodiscard]] const std::wstring &manufacturerString() const { return _manufacturerString; }

        [[nodiscard]] unsigned short vendorId() const { return _vendorId; }

        [[nodiscard]] unsigned short productId() const { return _productId; }

        [[maybe_unused]] int relayCount() const { return numRelays; }

        bool relayOpen(int num)
        {
            if (num >= relayCount())
                return true;
            uint8_t states = getStates();
            return states & (1<<num) == 0;
        }

        bool relayClosed(int num) {return !relayOpen(num);}

        void setRelayState(int num, bool closed)
        {
            if (num < numRelays)
                setState(num, closed ? (uint8_t) 0xff : (uint8_t) 0xfd);
        }

        void openRelay(int num)
        {
            if (num < numRelays)
                setState(num, 0xfd);
        }

        void closeRelay(int num)
        {
            if (num < numRelays)
                setState(num, 0xff);
        }

        void openAllRelays()
        {
            for (int i = 0; i < numRelays; ++i)
                openRelay(i);
        }

        void toggleRelay(int n)
        {
            if (relayClosed(n))
                openRelay(n);
            else
                closeRelay(n);
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
        static const int CmdGetFeatures = 1;
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
                printf(".calling.");
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
