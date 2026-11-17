#include "../src/modules/visual/spotify_source.hpp"
#include <cassert>
#include <iostream>
#include <limits>

namespace {
    struct counted {
        inline static int alive=0, copies=0;
        counted() { ++alive; }
        counted(const counted&) { ++alive; ++copies; }
        ~counted() { --alive; }
    };
}

int main()
{
    {
        modules::spotify::now_playing sample;
        sample.active=true; sample.playing=true; sample.position=100.; sample.duration=200.;
        sample.sampled_at=std::chrono::steady_clock::time_point{};
        auto a=sample,b=sample,c=sample;
        a.extrapolate(sample.sampled_at+std::chrono::milliseconds(4900));
        b.extrapolate(sample.sampled_at+std::chrono::milliseconds(5100));
        c.extrapolate(sample.sampled_at+std::chrono::seconds(30));
        assert(a.position>104.89 && b.position==105. && c.position==105.);
        sample.position=20.; sample.title="same title"; sample.artist="new artist";
        sample.extrapolate(sample.sampled_at+std::chrono::seconds(1));
        assert(sample.position==21.);
    }
    using namespace modules::spotify;
    now_playing published;
    published.active=true;
    published.artwork=std::vector<unsigned char>(640*640*4,127);
    published.lyrics={{0.,"first"},{2.,"second"},{2.,"same timestamp"},{5.,"last"}};
    const auto* pixels=published.artwork.data();
    const auto* lines=published.lyrics.data();
    // Many frames retain exactly one immutable pixel/lyric allocation.
    for (int frame=0;frame<100000;++frame) {
        auto snapshot=published;
        assert(snapshot.artwork.data()==pixels && snapshot.lyrics.data()==lines);
        snapshot.position=frame/1000.;
        int expected=-1;
        for (std::size_t i=0;i<snapshot.lyrics.size();++i) {
            if (snapshot.lyrics[i].timestamp<=snapshot.position+.05) expected=static_cast<int>(i);
            else break;
        }
        assert(snapshot.current_lyric()==expected);
    }
    auto old=published;
    published.artwork=std::vector<unsigned char>(16,255);
    published.lyrics.clear();
    assert(old.artwork.data()==pixels && old.artwork[0]==127 && old.lyrics.size()==4);
    assert(published.artwork.size()==16 && published.current_lyric()==-1);
    old.position=-1.; assert(old.current_lyric()==-1);
    old.position=2.; assert(old.current_lyric()==2);
    old.position=.25; assert(old.current_lyric()==0); // Backward seek.
    old.position=std::numeric_limits<double>::quiet_NaN(); assert(old.current_lyric()==-1);
    shared_media_buffer<counted> worker, reader;
    for (int track=0;track<10000;++track) {
        worker=std::vector<counted>(4);
        reader=worker;
        assert(counted::alive==4 && counted::copies==0);
        worker.clear(); assert(counted::alive==4);
        reader.clear(); assert(counted::alive==0);
    }
    std::cout << "Spotify: 100000 shared snapshots, seek/timestamp equivalence, 10000 replacements with complete reclamation passed\n";
}
