# ajylib

A personal C++ library focused on game server development.  
Built from scratch for portfolio, personal reuse, and learning purposes.

## Goals

- High-performance networking based on IOCP (Windows primary, epoll under consideration)
- Lock-free data structures and memory management
- Clean, reusable systems-level utilities

## Environment

- Language: C++20
- Platform: Windows (primary), Linux (under consideration)
- Architecture: x86-64
	- Some components rely on x86 TSO(Total Store Ordering) memory model
	- Some components rely on 48-bit canonical address form
- Build: CMake

## Status

Work in progress. Primarily intended for learning and experimentation.

---

게임 서버 개발에 초점을 맞춘 개인 C++ 라이브러리입니다.  
포트폴리오, 개인 재사용, 학습 목적으로 처음부터 직접 작성합니다.

## 목표

- IOCP 기반 고성능 네트워킹 (Windows 우선, epoll 추후 지원 고려 중)
- Lock-free 자료구조 및 메모리 관리
- 재사용 가능한 시스템 레벨 유틸리티

## 환경

- 언어: C++20
- 플랫폼: Windows (우선), Linux (고려 중)
- 아키텍처: x86-64
	- 일부 컴포넌트는 x86 TSO(Total Store Ordering) 메모리 모델에 의존
	- 일부 컴포넌트는 48비트 canonical address form에 의존
- 빌드: CMake

## 상태

개발 진행 중. 학습과 실험을 주 목적으로 합니다.