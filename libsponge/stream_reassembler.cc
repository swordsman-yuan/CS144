#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :   _ArrivalStrManager(),
                                                                _NextIndex(0),
                                                                _output(capacity), _capacity(capacity){}
                                                                

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) 
{
        /*0.special cases handler*/
        /*0.1 if the string arrived is empty*/
        /*only one situation needs to be handled, that is:*/
        /*the empty string only contains the eof message*/
        if(data.size() == 0 && eof == true)
        {       
                /*if the _ArrivalStrManager is empty*/
                /*and the _NextIndex equals to index, end the input stream and update the _NextIndex*/
                if(this->_ArrivalStrManager.size() == 0 && _NextIndex == index)
                {
                        this->_output.end_input();
                        ++this->_NextIndex;
                }
                else if(this->_ArrivalStrManager.size() > 0)
                {
                        size_t Size = this->_ArrivalStrManager.size();
                        if(this->_ArrivalStrManager[Size - 1]._Inteval.second + 1 == index)
                                this->_ArrivalStrManager[Size - 1]._Eof = true;
                }

                return;
        }

        /*0.2 if the string arrived is a duplicated one that has been received before*/
        /*just ignore it, ATTENTION: the string can not be empty*/
        if(index + data.size() <= _NextIndex)
                return;

        /* 1.truncate the newly-arrived string if needed */
        size_t RemainingCapacity = this->_output.remaining_capacity();          // how much space are left
        size_t LastReassembled = this->_NextIndex + RemainingCapacity - 1;      // calculate the last byte's index can be reassembled

        bool TruncatedFlag = false;  // flag indicating whether we have truncated string                                   
        std::string TruncatedData = "";
        if(index > LastReassembled)
         /* if the newly-arrived string has exceeded the LastReassembled byte */
         /* return directly */
                return;
        else
        {
                TruncatedData = data.substr(0, LastReassembled - index + 1);
                if(TruncatedData.size() < data.size())
                        TruncatedFlag = true;
        }

        /*2.trying to merge the newly arrived string*/
        /*construct the string*/
        std::string NewString = TruncatedData;                                                  // _Str
        std::pair<size_t, size_t> NewInteval(index, index + TruncatedData.size() - 1);          // _Inteval
        _SubString NewlyArrived(NewInteval, NewString, TruncatedFlag ? false : eof);           // call ctor

        /*push the substring into manager*/
        this->_ArrivalStrManager.push_back(NewlyArrived);

        /*trying to merge the newly-arrived string*/
        this->_ArrivalStrManager = this->mergeInteval();

        /*2.check whether we can push string into bytestream*/
        _SubString Front = this->_ArrivalStrManager.at(0);
        if(_NextIndex >= Front._Inteval.first)  // push new string into bytestream
        {       
                /*2.1 write the string into byte stream*/
                /*return the number of bytes actually written*/
                size_t ByteWrite = this->_output.write( Front._Str.substr(
                        _NextIndex - Front._Inteval.first,
                        Front._Str.size()
                ));

                /*2.2 update the _NextIndex if needed*/
                _NextIndex = _NextIndex + ByteWrite;

                /*2.3 ending the input stream if*/
                /*the last byte of Front._Str has been written into stream && the _Eof is true*/
                if(_NextIndex - 1 == Front._Inteval.second && Front._Eof == true)
                {
                        this->_output.end_input();
                        ++this->_NextIndex;
                }
                        
                /*2.4 erase the first element in _ArrivalStrManager*/
                this->_ArrivalStrManager.erase(this->_ArrivalStrManager.begin());
        }
}

/*ATTENTION: the substring should be merged before counted*/
/*ASSERTION: we don't need to invoke mergeInteval everytime*/
size_t StreamReassembler::unassembled_bytes() const // O(n)
{ 
        size_t Buffered = 0;
        size_t Size = this->_ArrivalStrManager.size();
        // scan the _ArrivalStrManager to calculate the space it has occupied
        for(size_t i = 0 ; i < Size ; ++i)
                Buffered += this->_ArrivalStrManager[i]._Str.size();

        return Buffered;
}

bool StreamReassembler::empty() const 
{ 
        return this->_ArrivalStrManager.size() == 0;
}

//! \brief trying merge the newly-arrived substring with the pre-existed substrings
/*I am inspired by leetcode 56 : merge intevals :)*/
vector<_SubString> StreamReassembler::mergeInteval() // O(nlogn)
{
        if(this->_ArrivalStrManager.size() <= 1)     // no need to merge, return directly
                return this->_ArrivalStrManager;

        /*after each merge operation, the _ArrivalStrManager will be updated*/
        vector<_SubString> NewManager;

        // 1.sort the inteval
        sort(   this->_ArrivalStrManager.begin(), this->_ArrivalStrManager.end(), 
                this->cmpFunction);

        // 2.scan the StrManager to detect whether we can merge substrings
        // merge inteval algorithm is applied here
        size_t Begin = this->_ArrivalStrManager[0]._Inteval.first;
        size_t End = this->_ArrivalStrManager[0]._Inteval.second;

        /*scan the _ArrivalStrManager to merge the intevals*/
        for(size_t i = 1 ;  i < this->_ArrivalStrManager.size() ; ++i)        
        {
                // no operations needed, update the [Begin, End] only
                if(this->_ArrivalStrManager[i]._Inteval.first > End + 1)                  
                {
                        NewManager.push_back(this->_ArrivalStrManager[i - 1]);
                        Begin = this->_ArrivalStrManager[i]._Inteval.first;
                        End = this->_ArrivalStrManager[i]._Inteval.second;
                }

                // need merge the substring and update the inteval
                else                                            
                {       
                        /*get the new bound of string*/
                        size_t NewEnd = End >= this->_ArrivalStrManager[i]._Inteval.second ?
                                        End : this->_ArrivalStrManager[i]._Inteval.second;
                        
                        /*the newly arrived string has been included in pre-existing string*/
                        if(NewEnd == End)
                        {
                                /*keep the updated substring in current substring*/
                                this->_ArrivalStrManager[i]._Inteval.first = Begin;
                                this->_ArrivalStrManager[i]._Inteval.second = End;
                                this->_ArrivalStrManager[i]._Str = this->_ArrivalStrManager[i - 1]._Str;
                                this->_ArrivalStrManager[i]._Eof = this->_ArrivalStrManager[i - 1]._Eof;
                        }
                        else
                        {
                                string MergedStr = this->_ArrivalStrManager[i - 1]._Str;
                                string StrTail = "";            // the substring needs to be appended

                                StrTail = this->_ArrivalStrManager[i]._Str.substr(
                                        this->_ArrivalStrManager[i]._Str.size() - (NewEnd - End),       /*pos*/
                                        this->_ArrivalStrManager[i]._Str.size()                         /*length*/  
                                );
                                MergedStr += StrTail;           // merge the string

                                // keep the updated information in current substring
                                this->_ArrivalStrManager[i]._Inteval.first = Begin;
                                this->_ArrivalStrManager[i]._Inteval.second = NewEnd;
                                this->_ArrivalStrManager[i]._Str = MergedStr;   
                                /*the _Eof flag should be kept unchanged, so ignore it here*/

                                // ATTENTION: don't forget to update the End
                                // it will be used in next iteration as a judgement
                                End = NewEnd;
                        }
                }
        }

        /*3.don't forget to handle the last substring*/
        NewManager.push_back(this->_ArrivalStrManager[this->_ArrivalStrManager.size() - 1]);

        // return std::move(NewManager);
        return NewManager;
}
